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
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <mad.h>

#define PRINTF_MODULE           "[control ] "

#define CMDLINE_MAX_LEN         512
#define BUFFER_SIZE             4000
#define SAMPLE_SIZE             (sizeof(int32_t))

#define NUM_SONGS_TO_DOWNLOAD   3
#define QUEUE_CLEAN_TIME        5

struct song_buffer {
        song_t *song;
        unsigned char *buf;
        size_t length;
};

static const char* RM_CMDLINE = "rm %s";
static const char* CACHE_DIR = "/root/cache/";
static const char* MP3_EXT = ".mp3";

static song_t *song_queue = NULL;

static int32_t *au_buf = NULL;          // decoded parts of music file
static int au_buf_start = 0;            // represents current index of audio data
static int au_buf_available = 0;        // represents the length of available data

static mad_timer_t progress = { .seconds = 0, .fraction = 0 };

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
        // TODO: not thread-safe
        // Go through first N songs and download them if not already downloaded
        song_t* current_song = song_queue;
        for (int i = 0; i < NUM_SONGS_TO_DOWNLOAD; i++) {
                if (!current_song) {
                        break;
                } 

                // Download the song in new thread
                if (current_song->status == CONTROL_SONG_STATUS_QUEUED)
                        downloader_queueDownloadSong(current_song);

                current_song = current_song->next;
        }
}

static void debugPrintSong(song_t* song) {

        // NOTE: not thread-safe, song could have been free'd
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
                        case CONTROL_SONG_STATUS_LOADED:
                                strcpy(statusStr, "LOADED");
                                break;
                        case CONTROL_SONG_STATUS_REMOVED:
                                strcpy(statusStr, "REMOVED");
                                break;
                }
                printf(PRINTF_MODULE "DEBUG - id=[%s], status=[%s], file=[%s]\n", song->vid, statusStr, song->filepath);
                (void)fflush(stdout);
        }
}

static void debugPrintSongList(void)
{
        // NOTE: not thread-safe, song could have been free'd

        printf(PRINTF_MODULE "DEBUG - printing current song queue\n");
        (void)fflush(stdout);

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
                length += (s->status == CONTROL_SONG_STATUS_QUEUED ||
                           s->status == CONTROL_SONG_STATUS_LOADED) ? 1 : 0;
                s = s->next;
        }
        pthread_mutex_unlock(&mtx_queue);

        return length;
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

static enum mad_flow madInputCallback(void *data, struct mad_stream *stream)
{
        struct song_buffer *sb = data;

        if (!sb->length)
                return MAD_FLOW_STOP;

        // work with the entire file, the output callback will handle decoder speed
        mad_stream_buffer(stream, sb->buf, sb->length);
        sb->length = 0;

        return MAD_FLOW_CONTINUE;
}

static enum mad_flow madOutputCallback(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
        unsigned int dual_channel, index;
        song_t *s;
        struct song_buffer *sb = data;
        struct timespec sleep_time = { .tv_nsec = 1 };

        // check if the song is still in the queue
        pthread_mutex_lock(&mtx_queue);

        s = song_queue;
        while (s) {
                if (s == sb->song) {
                        if (s->status != CONTROL_SONG_STATUS_LOADED)
                                s = NULL;
                        break;
                }
                s = s->next;
        }

        // if the song was removed from the queue, we can stop playing it
        if (!s) {
                pthread_mutex_unlock(&mtx_queue);
                return MAD_FLOW_STOP;
        }

        pthread_mutex_unlock(&mtx_queue);

        dual_channel = pcm->channels == 2 ? 1 : 0;

        while (index < pcm->length) {
                pthread_mutex_lock(&mtx_audio);

                // check if audio buffer is full
                if (au_buf_available >= BUFFER_SIZE)
                        goto cont;

                // fill as much of the audio buffer as possible
                while (au_buf_available < BUFFER_SIZE-1 && index < pcm->length) {
                        au_buf[au_buf_available % BUFFER_SIZE] = pcm->samples[0][index];
                        ++au_buf_available;
                        au_buf[au_buf_available % BUFFER_SIZE] = pcm->samples[dual_channel][index];
                        ++au_buf_available;

                        ++index;
                }
cont:
                pthread_mutex_unlock(&mtx_audio);
                (void)nanosleep(&sleep_time, NULL);
        }

        // update song progress (each frame should be roughly 26 ms), according to:
        // https://stackoverflow.com/questions/6220660/calculating-the-length-of-mp3-frames-in-milliseconds
        mad_timer_add(&progress, header->duration);

        return MAD_FLOW_CONTINUE;
}

static enum mad_flow madErrorCallback(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
        printf(PRINTF_MODULE "Warning: MP3 decoding error occurred (%s)\n",
                mad_stream_errorstr(stream));
        (void)fflush(stdout);

        return MAD_FLOW_CONTINUE;
}

static void *audioLoop(void *arg)
{
        unsigned int num_played, num_to_play;
        int32_t *buf;
        struct timespec sleep_time = { .tv_nsec = 1 };

        // prepare thread to be locked when first initialized (not playing)
        pthread_mutex_lock(&mtx_play);

        while (loop) {
                // sleep thread if not in playing status
                pthread_mutex_lock(&mtx_play);
                pthread_mutex_unlock(&mtx_play);

                pthread_mutex_lock(&mtx_audio);

                // check if audio data is available to be played
                // if not, we can sleep momentarily
                if (au_buf_available == 0) {
                        pthread_mutex_unlock(&mtx_audio);
                        (void)nanosleep(&sleep_time, NULL);
                        continue;
                }

                while (au_buf_available) {
                        buf = au_buf + au_buf_start;
                        num_to_play = au_buf_available + au_buf_start > BUFFER_SIZE ?
                                BUFFER_SIZE - au_buf_start :
                                au_buf_start + au_buf_available;

                        num_played = audio_playAudio(buf, num_to_play);

                        if (num_played < 0)
                                break;

                        au_buf_available -= num_played;
                        au_buf_start = (au_buf_start + num_played) % BUFFER_SIZE;
                }

                pthread_mutex_unlock(&mtx_audio);
        }

        printf(PRINTF_MODULE "Notice: shutting down audio playback thread\n");
        (void)fflush(stdout);

        return NULL;
}

static void *decoderLoop(void *arg)
{
        song_t *current_song;
        int fd;
        struct stat st;
        unsigned char *music_buf;
        struct song_buffer sb;
        struct mad_decoder decoder;

        while (loop) {
                if (!play_status) {
                        sleep(1);
                        continue;
                }

                pthread_mutex_lock(&mtx_queue);

                current_song = song_queue;

                // sleep briefly if the song queue is empty
                if (!current_song)
                        goto sleep;

                while (current_song) {
                        if (current_song->status == CONTROL_SONG_STATUS_LOADED ||
                            current_song->status == CONTROL_SONG_STATUS_QUEUED)
                                break;
                        current_song = current_song->next;
                }

                // all songs in the queue are removed or the first song
                // in the queue has not yet been downloaded
                if (!current_song || current_song->status == CONTROL_SONG_STATUS_QUEUED)
                        goto sleep;

                // load file into memory
                fd = open(current_song->filepath, O_RDONLY);
                // could not open file, retry?
                if (fd < 0)
                        goto sleep;

                // get file size
                if (fstat(fd, &st) == -1) {
                        (void)close(fd);
                        goto sleep;
                }

                music_buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
                if (music_buf == MAP_FAILED) {
                        (void)close(fd);
                        goto sleep;
                }

                printf(PRINTF_MODULE "Notice: starting decode of \"%s\"\n", current_song->filepath);
                (void)fflush(stdout);

                sb.song = current_song;

                pthread_mutex_unlock(&mtx_queue);

                sb.buf = music_buf;
                sb.length = st.st_size;

                // start decoding
                mad_decoder_init(&decoder, &sb, madInputCallback, NULL, NULL,
                        madOutputCallback, madErrorCallback, NULL);
                (void)mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
                mad_decoder_finish(&decoder);
                progress = mad_timer_zero;

                // close file
                (void)munmap(music_buf, st.st_size);
                (void)close(fd);

                // move onto the next song in the queue if repeat is not set
                if (!repeat_status) {
                        song_t *s;

                        pthread_mutex_lock(&mtx_queue);

                        // verify that the current song is still in the queue
                        s = song_queue;
                        while (s && s != current_song)
                                s = s->next;
                        if (s)
                                current_song->status = CONTROL_SONG_STATUS_REMOVED;

                        pthread_mutex_unlock(&mtx_queue);
                }

                continue;
sleep:
                pthread_mutex_unlock(&mtx_queue);
                (void)sleep(1);
        }

        printf(PRINTF_MODULE "Notice: shutting down decoder thread\n");
        (void)fflush(stdout);

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

                                        // no duplicates, safe to delete file
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
                                if (curr->next)
                                        curr->next->prev = curr->prev;

                                free(curr);
                        }

                        curr = next;
                }

sleep:
                pthread_mutex_unlock(&mtx_queue);
                (void)nanosleep(&sleep_time, NULL);
        }

        printf(PRINTF_MODULE "Notice: shutting down cleaner thread\n");
        (void)fflush(stdout);

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
                printf(PRINTF_MODULE "Error: unable to create thread for song queue cleanup\n");

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
}

void control_pauseAudio(void)
{
        // check if it is already not playing
        if (!play_status)
                return;

        pthread_mutex_lock(&mtx_play);
        play_status = 0;
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

        // skipping a playing song is equivalent to removing the first song
        control_removeSong(NULL, CONTROL_RMSONG_FIRST);

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
        // TODO: check if function this works with the new code
        song_t* new_song = malloc(sizeof(song_t));

        strcpy(new_song->vid, url);

        // Update song with .wav filepath
        strcpy(new_song->filepath, CACHE_DIR);
        strcat(new_song->filepath, new_song->vid);
        strcat(new_song->filepath, MP3_EXT);

        new_song->prev = NULL;
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
                new_song->prev = current_song;
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
        song_t *song;
        int i = 0;
        int err = 0;

        pthread_mutex_lock(&mtx_queue);

        song = song_queue;
        while (song) {
                if (song->status != CONTROL_SONG_STATUS_REMOVED)
                        i++;
                if ((index == CONTROL_RMSONG_FIRST && i != 0) || i > index)
                        break;
                song = song->next;
        }

        // we only need to mark the song as removed if it exists
        // the cleaner thread should do the actual removal
        if (song && ((i == index && !strcmp(url, song->vid)) ||
                     (index == CONTROL_RMSONG_FIRST)))
                song->status = CONTROL_SONG_STATUS_REMOVED;
        else
                err = 1;

        pthread_mutex_unlock(&mtx_queue);

        if (!err) {
                // Download new songs if needed
                updateDownloadedSongs();

                disp_setNumber(getSongQueueLength());
        }

        return err;
}

int control_getSongDetails(song_t *song, char *vid, char *filepath)
{
        // verify that the song is in the queue first
        song_t *s;

        pthread_mutex_lock(&mtx_queue);

        s = song_queue;
        while (s) {
                if (s == song) {
                        if (vid)
                                (void)strcpy(vid, song->vid);
                        if (filepath)
                                (void)strcpy(filepath, song->filepath);
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

enum control_song_status control_getFirstSongStatus(void)
{
        song_t *s;
        enum control_song_status status = CONTROL_SONG_STATUS_UNKNOWN;

        pthread_mutex_lock(&mtx_queue);
        s = song_queue;
        while (s) {
                if (s->status != CONTROL_SONG_STATUS_UNKNOWN ||
                    s->status != CONTROL_SONG_STATUS_REMOVED) {
                        status = s->status;
                        break;
                }
        }
        pthread_mutex_unlock(&mtx_queue);

        return status;
}

long control_getSongProgress(void)
{
        return progress.seconds;
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
        enum control_song_status status;

        status = control_setSongStatus(song, CONTROL_SONG_STATUS_LOADED);

        if (status != CONTROL_SONG_STATUS_LOADED)
                return status;

        pthread_mutex_lock(&mtx_queue);

        // only try to play the song automatically if the downloaded
        // song is at the front of the queue
        // NOTE: song could be removed by the time it reaches mutex, but shouldn't matter
        if (song == song_queue)
                control_playAudio();

        pthread_mutex_unlock(&mtx_queue);

        return status;
}
