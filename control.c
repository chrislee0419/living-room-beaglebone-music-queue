#include "control.h"

#include "network.h"
#include "audio.h"
#include "downloader.h"
#include "main.h"
#include "disp.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <mad.h>

#define PRINTF_MODULE           "[control ] "

#define CMDLINE_MAX_LEN         512
#define BUFFER_SIZE             2000
#define SAMPLE_SIZE             (sizeof(short))

#define NUM_SONGS_TO_DOWNLOAD   3
#define QUEUE_CLEAN_TIME        5

static const char* RM_CMDLINE = "rm %s";
static const char* CACHE_DIR = "/root/cache/";
static const char* MP3_EXT = ".mp3";

static song_t *song_queue = NULL;
static song_t *prev_songs[CONTROL_PREV_SONGS_LIST_LEN] = {0};

static short *au_buf = NULL;            // entire music file
static int au_buf_start = 0;            // represents current index of audio data
static int au_buf_end = 0;              // represents index after valid audio data

// playback control on master device
static int repeat_status = 0;
static int play_status = 0;
static pthread_mutex_t mtx_play  = PTHREAD_MUTEX_INITIALIZER;

static int loop = 0;
static pthread_t th_aud;
static pthread_t th_dec;
static pthread_t th_clr;

static pthread_mutex_t mtx_audio = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_queue = PTHREAD_MUTEX_INITIALIZER;

/*
 * Helper functions
 */

// Checks if any songs need to be downloaded and processed
// Downloads them in a new background thread
static void updateDownloadedSongs(void)
{
        // Go through first N songs and download them if not already downloaded
        song_t* current_song = song_queue;
        for (int i = 0; i < NUM_SONGS_TO_DOWNLOAD; i++) {
                if (!current_song) {
                        break;
                } 

                if (current_song->status == CONTROL_SONG_STATUS_QUEUED) {
                        // Download the song in new thread
                        downloader_queueDownloadSong(current_song);
                }
                else if (current_song->status == CONTROL_SONG_STATUS_REMOVED) {
                        // Tried to remove song while downloading
                        // Remove it here now
                        control_removeSong(current_song->vid, CONTROL_RMSONG_FIRST);
                }

                current_song = current_song->next;
        }
}

static void debugPrintSong(song_t* song) {

        if (song == NULL) {
                printf("NULL song\n");
        } else {
                char statusStr[10];
                switch (song->status) {
                        case CONTROL_SONG_STATUS_UNKNOWN:
                                strcpy(statusStr, "UNKNOWN");
                                break;
                        case CONTROL_SONG_STATUS_QUEUED:
                                strcpy(statusStr, "QUEUED");
                                break;
                        case CONTROL_SONG_STATUS_LOADING:
                                strcpy(statusStr, "LOADING");
                                break;
                        case CONTROL_SONG_STATUS_LOADED:
                                strcpy(statusStr, "LOADED");
                                break;
                        case CONTROL_SONG_STATUS_REMOVED:
                                strcpy(statusStr, "REMOVED");
                                break;
                        case CONTROL_SONG_STATUS_PLAYING:
                                strcpy(statusStr, "PLAYING");
                                break;
                }
                printf("id=[%s], status=[%s], file=[%s]\n", song->vid, statusStr, song->filepath);
        }
}

static void debugPrintSongList(void)
{
        song_t* current_song = song_queue;
        while (current_song) {
                debugPrintSong(current_song);
                current_song = current_song->next;
        }
}

static int getSongQueueLength(void)
{
        int length = 0;
        song_t *s;

        pthread_mutex_lock(&mtx_queue);
        s = song_queue;
        while (s) {
                s = s->next;
                length++;
        }
        pthread_mutex_unlock(&mtx_queue);

        return length;
}

void control_deleteAndFreeSong(song_t* song) {

        printf(PRINTF_MODULE "Info: Deleting song - ");
        debugPrintSong(song);

        if (!song) return;

        bool shouldRemoveFromQueue = false;
        bool shouldDeleteFile = false;
        bool shouldFree = false;

        if (song->status == CONTROL_SONG_STATUS_QUEUED) {
                shouldRemoveFromQueue = true;
                shouldFree = true;
        }
        else if (song->status == CONTROL_SONG_STATUS_LOADING) {
                // Dangerous to delete while downloading
                // Set status to delete later by the Downloader
                control_setSongStatus(song, CONTROL_SONG_STATUS_REMOVED);
                shouldRemoveFromQueue = true;
        }
        else if (song->status == CONTROL_SONG_STATUS_LOADED
                || song->status == CONTROL_SONG_STATUS_PLAYING) {

                shouldRemoveFromQueue = true;
                shouldDeleteFile = true;
                shouldFree = true;
        }
        else if (song->status == CONTROL_SONG_STATUS_REMOVED) {
                shouldDeleteFile = true;
                shouldFree = true;
        }

        if (shouldDeleteFile) {
                // Delete wav file on disk
                // only delete if the song does not appear again in the queue
                bool isSongDuplicated = false;
                song_t* curr_song = song_queue;
                while (curr_song) {
                        if (strcmp(curr_song->vid, song->vid) == 0 && curr_song != song) {
                                isSongDuplicated = true;
                                break;
                        }
                        curr_song = curr_song->next;
                }

                if (!isSongDuplicated) {
                        downloader_deleteSongFile(song);
                }
        }

        if (shouldRemoveFromQueue) {
                pthread_mutex_lock(&mtx_queue);
                // If deleting the first song, update the song_queue
                if (song == song_queue) {
                        song_queue = song->next;
                }
                else {      
                        // Otherwise find predecessor and make it point to the next song  
                        song_t* prev_song = song_queue;
                        while(prev_song && prev_song->next != song) {
                                prev_song = prev_song->next;
                        }

                        if (prev_song) {
                                prev_song->next = song->next;
                        }
                }
                pthread_mutex_unlock(&mtx_queue);

                updateDownloadedSongs();
        }

        if (shouldFree) {
                free(song);
        }
}

static void clearSongQueue(void)
{
        song_t *s;

        pthread_mutex_lock(&mtx_queue);
        while (song_queue) {
                s = song_queue;
                song_queue = s->next;
                free(s);
        }
        pthread_mutex_unlock(&mtx_queue);
}

enum mad_flow madInputCallback(void *data, struct mad_stream *stream)
{
        return MAD_FLOW_CONTINUE;
}

enum mad_flow madOutputCallback(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
        return MAD_FLOW_CONTINUE;
}

enum mad_flow madErrorCallback(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
        return MAD_FLOW_CONTINUE:
}

static void *audioLoop(void *arg)
{
        unsigned int num_played;
        short *buf;

        // prepare thread to be locked when first initialized (not playing)
        pthread_mutex_lock(&mtx_play);

        while (loop) {
                // sleep thread if not in playing status
                pthread_mutex_lock(&mtx_play);
                pthread_mutex_unlock(&mtx_play);

                pthread_mutex_lock(&mtx_audio);

                // take new song from the top of the queue if:
                // - au_buf (audio data from file) is NULL
                // - end of the current song is reached
                if (!au_buf || au_buf_end-1 <= au_buf_start) {
                        // repeat song if set
                        if (au_buf && repeat_status && song_queue) {
                                au_buf_start = 0;
                        } else {
                                // unlock the audio mutex, as loadNewSong will need it
                                pthread_mutex_unlock(&mtx_audio);
                                if (loadNewSong()) {
                                        // if queue is empty/error occurred, pause audio playback
                                        control_pauseAudio();
                                        continue;
                                }
                                pthread_mutex_lock(&mtx_audio);
                        }
                }

                if (!au_buf) {
                        pthread_mutex_unlock(&mtx_audio);
                        control_pauseAudio();
                        continue;
                }

                buf = au_buf + au_buf_start;
                num_played = audio_playAudio(buf, au_buf_end - au_buf_start);

                if (num_played < 0) {
                        pthread_mutex_unlock(&mtx_audio);
                        continue;
                }

                au_buf_start += num_played;

                pthread_mutex_unlock(&mtx_audio);
        }

        return NULL;
}

static void *decoderLoop(void *arg)
{
        struct timespec sleep_time = { .tv_nsec = 1 };

        while (loop) {
                pthread_mutex_lock(&mtx_queue);

                // sleep briefly if the song queue is empty
                if (!song_queue) {
                        pthread_mutex_unlock(&mtx_queue);
                        (void)nanosleep(&sleep_time, NULL);
                        continue;
                }
        }

        return NULL;
}

static void *queueCleanerLoop(void *arg)
{
        struct timespec sleep_time = { .tv_sec = QUEUE_CLEAN_TIME };
        char cmdline[CMDLINE_MAX_LEN];
        song_t *curr = NULL;
        song_t *next = NULL;

        while (loop) {
                pthread_mutex_lock(&mtx_queue);

                if (!song_queue)
                        goto sleep;

                curr = song_queue;
                while (curr) {
                        next = curr->next;

                        if (curr->status == CONTROL_SONG_STATUS_REMOVED) {
                                // remove the song file if it exists
                                // should be safe to use while song is downloading
                                if (!access(curr->filepath, F_OK)) {
                                        // only remove the song file if the song isn't
                                        // duplicated in the queue
                                        song_t *s = curr->next;

                                        while (s) {
                                                if (strcmp(curr->vid, s->vid) == 0)
                                                        break;
                                                s = s->next;
                                        }

                                        // no duplicate, safe to delete file
                                        if (!s) {
                                                sprintf(cmdline, RM_CMDLINE, curr->filepath);
                                                system(cmdline);
                                        }
                                }

                                // remove song from queue
                                if (song_queue == curr)
                                        song_queue = next;
                                else
                                        curr->prev->next = curr->next;
                                curr->next->prev = curr->prev;

                                free(curr);
                        }

                        curr = next;
                }

sleep:
                pthread_mutex_unlock(&mtx_queue);
                (void)nanosleep(&sleep_time, NULL);
        }

        return NULL;
}

/*
 * Public functions
 */
int control_init(void)
{
        int err = 0;

        loop = 1;

        downloader_init();

        if ((err = pthread_create(&th_aud, NULL, audioLoop, NULL)))
                printf(PRINTF_MODULE "Error: unable to create thread to play audio\n");
        else if ((err = pthread_create(&th_dec, NULL, decoderLoop, NULL)))
                printf(PRINTF_MODULE "Error: unable to create thread for MP3 decoding\n");
        else if ((err = pthread_create(&th_clr, NULL, queueCleanerLoop, NULL)))
                printf(PRINTF_MODULE "Error: unable to create thread for song queue management\n");

        return err;
}

void control_cleanup(void)
{
        loop = 0;

        (void)pthread_join(th_aud, NULL);
        (void)pthread_join(th_dec, NULL);
        (void)pthread_join(th_clr, NULL);

        downloader_cleanup();

        // clear audio queue
        clearSongQueue();

        // destroy audio data buffer
        pthread_mutex_lock(&mtx_audio);
        if (au_buf) {
                free(au_buf);
                au_buf = NULL;
        }
        pthread_mutex_unlock(&mtx_audio);
}

void control_playAudio(void)
{
        // check if it is already playing
        if (play_status)
                return;

        // NOTE: play_status change must occur after changing the mutex
        pthread_mutex_unlock(&mtx_play);
        play_status = 1;

        // if buffer is empty, next song should be enqueued by audioLoop
}

void control_pauseAudio(void)
{
        if (mode == CONTROL_MODE_MASTER) {
                // check if it is already not playing
                if (!play_status)
                        return;

                pthread_mutex_lock(&mtx_play);
                play_status = 0;
        }
}

int control_getPlayStatus(void)
{
        return play_status;
}

void control_skipSong(void)
{
        int was_playing = 0;

        if (play_status) {
                control_pauseAudio();
                was_playing = 1;
        }

        // TODO: Handle skipping when song is still loading
        // TODO: Reset audio to 0
        control_deleteAndFreeSong(song_queue);
        disp_setNumber(getSongQueueLength());

        // Delete the current buf, so we're forced to load a new one
        pthread_mutex_lock(&mtx_audio);
        if (au_buf)
                free(au_buf);
                au_buf = NULL;
        au_buf_start = 0;
        pthread_mutex_unlock(&mtx_audio);

        // resume audio if it was playing before
        if (was_playing) {
                control_playAudio();
        }
}

void control_setRepeatStatus(int repeat)
{
        repeat_status = repeat;
}

int control_getRepeatStatus(void)
{
        return repeat_status;
}

void control_addSong(char *url)
{
        song_t* new_song = malloc(sizeof(song_t));

        strcpy(new_song->vid, url);

        // Update song with .wav filepath
        strcpy(new_song->filepath, CACHE_DIR);
        strcat(new_song->filepath, new_song->vid);
        strcat(new_song->filepath, MP3_EXT);

        new_song->next = NULL;
        new_song->status = CONTROL_SONG_STATUS_QUEUED;

        // Add to end of list
        pthread_mutex_lock(&mtx_queue);
        if (!song_queue) {
                song_queue = new_song;
        } else {
                song_t *current_song = song_queue;
                while (current_song->next != NULL) {
                        current_song = current_song->next;
                }

                current_song->next = new_song;
        }

        pthread_mutex_unlock(&mtx_queue);

        // Download songs if needed
        updateDownloadedSongs();

        // Show the number of songs in the queue on the display
        disp_setNumber(getSongQueueLength());

        debugPrintSongList();
}

int control_removeSong(char *url, int index)
{
        // TODO
        printf(PRINTF_MODULE "Notice: control_removeSong() got %s, %d\n", url, index);
        (void)fflush(stdout);

        // Find song in list
        // use pthread_mutex_lock(&mtx_queue) when modifying song_curr or song_queue

        // Delete file from disk

        // free() song object only after we have confirmed it is not in the downloader's queue

        // Download new songs if needed
        updateDownloadedSongs();

        disp_setNumber(getSongQueueLength());

        return 0;
}

int control_getSongFilepath(song_t *song, char **buf)
{
        // verify that the song is in the queue first
        song_t *s;

        pthread_mutex_lock(&mtx_queue);

        s = song_queue;
        while (s) {
                if (s == song) {
                        (void)strcpy(*buf, song->filepath);
                        pthread_mutex_unlock(&mtx_queue);
                        return 0;
                }
                s = s->next;
        }

        pthread_mutex_unlock(&mtx_queue);

        return 1;
}

enum control_song_status control_setSongStatus(song_t *song, enum control_song_status status)
{
        song_t *s;

        // prevent changing status to unknown state
        if (status == CONTROL_SONG_STATUS_UNKNOWN)
                return status;

        pthread_mutex_lock(&mtx_queue);

        // verify that the song is in the queue
        s = song_queue;
        while (s != song && s != NULL)
                s = s->next;
        if (!s) {
                pthread_mutex_unlock(&mtx_queue);
                return CONTROL_SONG_STATUS_UNKNOWN;
        }

        // if the song is set to be removed, we return that instead
        if (s->status == CONTROL_SONG_STATUS_REMOVED) {
                pthread_mutex_unlock(&mtx_queue);
                return CONTROL_SONG_STATUS_REMOVED;
        }

        s->status = status;
        pthread_mutex_unlock(&mtx_queue);

        return status;
}

void control_getSongProgress(int *curr, int *end)
{
        // TODO: needs to be updated for mp3s
        *curr = au_buf_start;
        *end = au_buf_end;
}

char *control_getQueueVIDs(void)
{
        char *buf, *c;
        song_t *s;
        unsigned int length, bytes;

        pthread_mutex_lock(&mtx_queue);

        s = song_queue;
        length = 0;
        while (s) {
                ++length;
                s = s->next;
        }

        // we still want it to produce an empty string if there
        // are no songs in the queue
        if (!length)
                length = 1;

        buf = malloc((CONTROL_MAXLEN_VID+1) * length);
        if (!buf)
                goto out;
        (void)memset(buf, 0, (CONTROL_MAXLEN_VID+1) * length);

        s = song_queue;
        c = buf;
        while (s) {
                bytes = sprintf(c, "%s,", s->vid);
                if (!bytes) {
                        printf(PRINTF_MODULE "Error: unable to create queue ids string\n");
                        free(buf);
                        buf = NULL;
                        goto out;
                }

                c += bytes;
                s = s->next;
        }

        // remove the last comma
        if (c > buf)
                *(c-1) = '\0';

out:
        pthread_mutex_unlock(&mtx_queue);

        return buf;
}

int control_onDownloadComplete(song_t* song)
{
        song_t *s;
        enum control_song_status status;

        status = control_setSongStatus(song, CONTROL_SONG_STATUS_LOADED);

        if (status != CONTROL_SONG_STATUS_LOADED)
                return status;

        pthread_mutex_lock(&mtx_queue);

        // only try to play the song automatically if the downloaded
        // song is at the front of the queue
        if (song == song_queue)
                control_playAudio();

        pthread_mutex_unlock(&mtx_queue);
}
