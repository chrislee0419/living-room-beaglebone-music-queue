#include "control.h"

#include "network.h"
#include "audio.h"
#include "downloader.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define PRINTF_MODULE           "[control ] "

#define DATA_OFFSET_INTO_WAVE   44
#define SAMPLE_SIZE             (sizeof(short))

#define SLAVE_BUF_SIZE          2000
#define PING_DELAY_S            3
#define PING_CHECK_FREQ         3

#define NUM_SONGS_TO_DOWNLOAD 3

typedef struct slave_dev {
        int active;
        struct sockaddr_in addr;
        struct slave_dev *next;
} slave_dev_t;

static enum control_mode mode = CONTROL_MODE_MASTER;
static slave_dev_t *slave_dev_list = NULL;

static song_t *song_queue = NULL;

static short *au_buf = NULL;            // ring buffer when in slave mode, otherwise entire music file
static int au_buf_start = 0;            // represents current index of audio data
static int au_buf_end = 0;              // represents index after valid audio data

// playback control on master device
static int repeat_status = 0;
static int play_status = 0;
static pthread_mutex_t mtx_play  = PTHREAD_MUTEX_INITIALIZER;

static int loop = 0;
static pthread_t th_aud;
static pthread_t th_ping;
static pthread_mutex_t mtx_audio = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_queue = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_slist = PTHREAD_MUTEX_INITIALIZER;

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

                if (current_song->status == CONTROL_SONG_STATUS_LOADING
                        || current_song->status == CONTROL_SONG_STATUS_LOADED) {
                        // Do nothing
                }
                else if (current_song->status == CONTROL_SONG_STATUS_QUEUED) {
                        // Download the song in new thread
                        downloader_queueDownloadSong(current_song);

                        // Update song status
                        current_song->status = CONTROL_SONG_STATUS_LOADING;
                }
                else if (current_song->status == CONTROL_SONG_STATUS_REMOVED) {
                        // Tried to remove song while downloading
                        // Remove it here now
                        control_removeSong(current_song->vid);
                }

                current_song = current_song->next;
        }
}

static void debugPrintSongList(void)
{
        song_t* current_song = song_queue;
        while (current_song) {
                char statusStr[10];
                switch (current_song->status) {
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
                }
                printf("file=[%s], id=[%s], status=[%s]\n", current_song->filepath, current_song->vid, statusStr);
                current_song = current_song->next;
        }
}

static void deleteAndFreeSong(song_t* song) {
        if (song->status == CONTROL_SONG_STATUS_LOADED) {
                // TODO: delete wav file on disk
                // only delete if the song does not appear again in the queue
        }

        free(song);
}

// Called when a song is done playing
static int loadNewSong(void)
{
        FILE *file;
        int sizeInBytes;
        int samplesRead;
        int bufEnd;
        short *buf;

        // get new song from queue
        pthread_mutex_lock(&mtx_queue);
        if (!song_queue)
                return ENODATA;

        song_t* song_prev = song_queue;

        // Get a new current song at the front of the queue
        song_queue = song_queue->next;
        song_t* song_curr = song_queue;

        deleteAndFreeSong(song_prev);

        // check if the audio file has been downloaded
        if (song_curr->status != CONTROL_SONG_STATUS_LOADED) {
                pthread_mutex_unlock(&mtx_queue);
                return ENODATA;
        }

        pthread_mutex_unlock(&mtx_queue);

        // Open file
        file = fopen(song_curr->filepath, "r");
        if (file == NULL) {
                printf(PRINTF_MODULE "Warning: Unable to open file %s.\n", song_curr->filepath);
                (void)fflush(stdout);
                return EIO;
        }

        // Get file size
        fseek(file, 0, SEEK_END);
        sizeInBytes = ftell(file) - DATA_OFFSET_INTO_WAVE;
        fseek(file, DATA_OFFSET_INTO_WAVE, SEEK_SET);
        bufEnd = sizeInBytes / SAMPLE_SIZE;

        // Allocate Space
        buf = malloc(sizeInBytes);
        if (buf == NULL) {
                printf(PRINTF_MODULE "Warning: Unable to allocate %d bytes for file %s.\n",
                        sizeInBytes, song_curr->filepath);
                (void)fflush(stdout);
                fclose(file);
                return ENOMEM;
        }

        // Read data:
        samplesRead = fread(buf, SAMPLE_SIZE, bufEnd, file);
        if (samplesRead != bufEnd) {
                printf(PRINTF_MODULE "Warning: Unable to read %d samples from file %s (read %d).\n",
                        bufEnd, song_curr->filepath, samplesRead);
                fclose(file);
                free(buf);
                return EIO;
        }

        fclose(file);

        // copy data to au_buf
        pthread_mutex_lock(&mtx_audio);
        if (au_buf)
                free(au_buf);
        au_buf = buf;
        au_buf_start = 0;
        au_buf_end = bufEnd;
        pthread_mutex_unlock(&mtx_audio);

        return 0;
}

static void sendAudioToSlave(short *aubuf, unsigned int size)
{
        char *buf;
        unsigned int buf_size;
        slave_dev_t *s;

        buf_size = sizeof(char) * SAMPLE_SIZE * size + strlen("audio\n");
        buf = malloc(buf_size);
        if (!buf) {
                printf(PRINTF_MODULE "Warning: unable to allocate memory for slave audio buffer\n");
                (void)fflush(stdout);
                return;
        }

        // copy audio data, prepended by the "audio" command
        (void)strcpy(buf, "audio\n");
        (void)memcpy(buf + strlen("audio\n"), aubuf, size);

        pthread_mutex_lock(&mtx_slist);
        s = slave_dev_list;
        while (s) {
                network_sendData(buf, buf_size, s->addr);
                s = s->next;
        }
        pthread_mutex_unlock(&mtx_slist);
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

static void *audioLoop(void *arg)
{
        unsigned int num_played;
        short *buf;

        // prepare thread to be locked when first initialized (not playing)
        // only applies to master, slave should always play
        if (mode == CONTROL_MODE_MASTER)
                pthread_mutex_lock(&mtx_play);

        while (loop) {
                // sleep thread if not in playing status
                pthread_mutex_lock(&mtx_play);
                pthread_mutex_unlock(&mtx_play);

                // take new song from the top of the queue if:
                // - au_buf (audio data from file) is NULL
                // - end of the current song is reached
                if (!au_buf || au_buf_end == au_buf_start ) {
                        // busy waiting during slave mode
                        // audio data can come from master at any time
                        if (mode == CONTROL_MODE_SLAVE)
                                continue;

                        // take the next song from the queue
                        if (loadNewSong()) {
                                // if queue is empty/error occurred, pause audio playback
                                control_pauseAudio();
                                continue;
                        }
                }

                // play audio
                pthread_mutex_lock(&mtx_audio);

                if (!au_buf) {
                        pthread_mutex_unlock(&mtx_audio);
                        control_pauseAudio();
                        continue;
                }

                // handle circular buffer for slave
                if (mode == CONTROL_MODE_SLAVE && au_buf_start > au_buf_end) {
                        buf = au_buf + au_buf_start;
                        num_played = audio_playAudio(buf, SLAVE_BUF_SIZE - au_buf_start - 1);

                        if (num_played < 0) {
                                pthread_mutex_unlock(&mtx_audio);
                                continue;
                        }

                        au_buf_start = 0;
                } else if (mode == CONTROL_MODE_UNKNOWN) {
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

                // send audio to slave devices
                if (mode == CONTROL_MODE_SLAVE)
                        sendAudioToSlave(buf, num_played);

                au_buf_start += num_played;
                pthread_mutex_unlock(&mtx_audio);
        }

        return NULL;
}

static void *pingLoop(void *arg)
{
        int counter = 0;
        slave_dev_t *s;

        while (loop) {
                s = slave_dev_list;

                while (s) {
                        network_sendPing(s->addr);
                        s = s->next;
                }

                counter++;
                sleep(PING_DELAY_S);

                if (counter >= PING_CHECK_FREQ) {
                        slave_dev_t *prev = NULL;

                        counter = 0;

                        // check if all slave devices are still active
                        pthread_mutex_lock(&mtx_slist);
                        s = slave_dev_list;

                        while (s) {
                                // remove inactive slave
                                if (!s->active) {
                                        // check if it is the head of the linked list
                                        if (!prev) {
                                                slave_dev_list = s->next;
                                                free(s);
                                                s = slave_dev_list;
                                        } else {
                                                prev->next = s->next;
                                                free(s);
                                        }

                                        continue;
                                }

                                prev = s;
                                s = s->next;
                        }
                        pthread_mutex_unlock(&mtx_slist);
                }
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
                printf(PRINTF_MODULE "Error: unable to create thread for audio\n");
        else if ((err = pthread_create(&th_ping, NULL, pingLoop, NULL)))
                printf(PRINTF_MODULE "Error: unable to create thread for pinging slave devices\n");

        return err;
}

void control_cleanup(void)
{
        slave_dev_t *s;

        loop = 0;

        (void)pthread_join(th_aud, NULL);
        (void)pthread_join(th_ping, NULL);

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

        // clear slave list
        pthread_mutex_lock(&mtx_slist);
        s = slave_dev_list;
        while (s) {
                slave_dev_list = s->next;
                free(s);
                s = slave_dev_list;
        }
        pthread_mutex_unlock(&mtx_slist);
}

void control_setMode(enum control_mode m)
{
        // NOTE: "connect" command should be sent after this in the network module
        mode = m;

        control_pauseAudio();
        audio_stopAudio();

        clearSongQueue();

        pthread_mutex_lock(&mtx_audio);
        if (au_buf) {
                free(au_buf);
                au_buf = NULL;
        }

        au_buf_start = 0;
        au_buf_end = 0;
        pthread_mutex_unlock(&mtx_audio);

        if (m == CONTROL_MODE_SLAVE) {
                // set static buf for slave device
                pthread_mutex_lock(&mtx_audio);
                au_buf = malloc(SLAVE_BUF_SIZE);
                if (!au_buf) {
                        printf(PRINTF_MODULE "Error: unable to allocate memory for audio buffer while setting slave mode\n");
                        main_triggerShutdown();
                }
                pthread_mutex_unlock(&mtx_audio);
        }
}

enum control_mode control_getMode(void)
{
        return mode;
}

void control_queueAudio(char *buf, unsigned int length)
{
        int available;

        if (mode != CONTROL_MODE_SLAVE)
                return;

        pthread_mutex_lock(&mtx_audio);
        if (!au_buf) {
                pthread_mutex_unlock(&mtx_audio);
                return;
        }

        // check if the entirety of the received buffer will fit in the audio buffer
        // otherwise we can only store some of the received audio
        if (au_buf_start <= au_buf_end)
                available = au_buf_end - au_buf_start;
        else
                available = SLAVE_BUF_SIZE - au_buf_start + au_buf_end;

        length = available < length ? available : length;

        // copy received audio data
        if (au_buf_end + length > SLAVE_BUF_SIZE) {
                (void)memcpy(au_buf + au_buf_end, buf, SLAVE_BUF_SIZE - au_buf_end);
                length = length - SLAVE_BUF_SIZE - au_buf_end;
                au_buf_end = 0;
        }

        (void)memcpy(au_buf + au_buf_end, buf, length);
        au_buf_end += length;

        pthread_mutex_unlock(&mtx_audio);
}

void control_playAudio(void)
{
        if (mode == CONTROL_MODE_MASTER) {
                // check if it is already playing
                if (play_status)
                        return;

                // NOTE: play_status change must occur after changing the mutex
                pthread_mutex_unlock(&mtx_play);
                play_status = 1;

                // if buffer is empty, next song should be enqueued by audioLoop
        }
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
        if (mode == CONTROL_MODE_MASTER) {
                int was_playing = 0;

                if (play_status) {
                        control_pauseAudio();
                        was_playing = 1;
                }

                // free the current song if playing, or the next song in the queue
                pthread_mutex_lock(&mtx_queue);
                if (song_queue) {
                        song_t *s;

                        s = song_queue;
                        song_queue = s->next;
                        free(s);
                }
                pthread_mutex_unlock(&mtx_queue);

                // resume audio if it was playing before
                if (was_playing)
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

        strcpy(new_song->filepath, "");
        strcpy(new_song->vid, url);

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

        // Download songs if needed
        updateDownloadedSongs();
        pthread_mutex_unlock(&mtx_queue);

        debugPrintSongList();
}

void control_removeSong(char *url)
{
        // TODO
        printf(PRINTF_MODULE "Notice: control_removeSong() got %s\n", url);
        (void)fflush(stdout);

        // Find song in list
        // use pthread_mutex_lock(&mtx_queue) when modifying song_curr or song_queue

        // Delete file from disk

        // free() song object only after we have confirmed it is not in the downloader's queue

        // Download new songs if needed
        updateDownloadedSongs();
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

const song_t *control_getQueue(void)
{
        return song_queue;
}

void control_addSlave(struct sockaddr_in addr)
{
        slave_dev_t *s = malloc(sizeof(slave_dev_t));
        if (!s) {
                printf(PRINTF_MODULE "Warning: unable to add slave device\n");
                return;
        }

        s->active = 1;
        s->addr = addr;

        pthread_mutex_lock(&mtx_slist);
        s->next = slave_dev_list;
        slave_dev_list = s;
        pthread_mutex_unlock(&mtx_slist);
}

void control_verifySlaveStatus(struct sockaddr_in addr)
{
        pthread_mutex_lock(&mtx_slist);
        slave_dev_t *s = slave_dev_list;
        while (s) {
                if (s->addr.sin_addr.s_addr == addr.sin_addr.s_addr) {
                        s->active = 1;
                        pthread_mutex_unlock(&mtx_slist);
                        return;
                }
        }

        pthread_mutex_unlock(&mtx_slist);

        printf(PRINTF_MODULE "Warning: unable to verify slave status, no slave with the provided address\n");
}


void control_onDownloadComplete(void) {
        control_playAudio();
}
