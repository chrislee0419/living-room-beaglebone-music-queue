#include "downloader.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PRINTF_MODULE   "[download] "

#define CMDLINE_MAX_LEN 1024
static const char* DOWNLOAD_CMDLINE = "youtube-dl --extract-audio --audio-format wav -o '~/cache/%%(id)s.%%(ext)s' --postprocessor-args \"-ar 44100\" https://www.youtube.com/watch?v=%s";
static const char* RM_CACHE_CMDLINE = "rm ~/cache/*";
static const char* WAV_EXT = ".wav";

#define FIFO_QUEUE_SIZE 5
static song_t* songFifoQueue[FIFO_QUEUE_SIZE];
static int fifoHeadIndex = 0;
static int fifoTailIndex = 0;	// The next empty slot, at the end of the queue
static pthread_mutex_t fifoMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t isDownloadingMutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Forward declations
 */
static void* downloadThread();
static void enqueueSong();
static song_t* dequeueSong();


/*
 * Public functions
 */
void downloader_init(void) 
{
	pthread_mutex_init(&fifoMutex, NULL);

	// Clean FIFO queue
	pthread_mutex_lock(&fifoMutex);
	memset(songFifoQueue, 0, FIFO_QUEUE_SIZE * sizeof(songFifoQueue));
        fifoHeadIndex = 0;
        fifoTailIndex = 0;
	pthread_mutex_unlock(&fifoMutex);

	// Clear cache
	system(RM_CACHE_CMDLINE);
}

void downloader_cleanup(void) 
{
	// Clear cache
	system(RM_CACHE_CMDLINE);
}

void downloader_queueDownloadSong(song_t* song) 
{
	enqueueSong(song);

	// Spawn download thread if it's not running
	if (!pthread_mutex_trylock(&isDownloadingMutex)) {
		pthread_t pDownloadThread;
                pthread_attr_t attr;

                // set detached thread state, so join is not necessary for cleanup
                (void)pthread_attr_init(&attr);
                (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		(void)pthread_create(&pDownloadThread, &attr, &downloadThread, 0);
                (void)pthread_attr_destroy(&attr);
	}
}


/*
 * Private functions
 */

static void enqueueSong(song_t* song)
{
	pthread_mutex_lock(&fifoMutex);
	if (songFifoQueue[fifoTailIndex] != NULL) {
		printf(PRINTF_MODULE "Warning: Download queue full - item not queued\n");
	        pthread_mutex_unlock(&fifoMutex);
		return;
	}

        // Check if the file exists already
        if (!access(song->filepath, F_OK)) {
                printf(PRINTF_MODULE "Notice: music file already exists, item not queued\n");
                pthread_mutex_unlock(&fifoMutex);
                return;
        }

	songFifoQueue[fifoTailIndex] = song;
	fifoTailIndex = (fifoTailIndex + 1) % FIFO_QUEUE_SIZE;
	pthread_mutex_unlock(&fifoMutex);
}

static song_t* dequeueSong()
{
	if (fifoHeadIndex == fifoTailIndex) {
		return NULL;
	}

	pthread_mutex_lock(&fifoMutex);
	song_t* output = songFifoQueue[fifoHeadIndex];
	songFifoQueue[fifoHeadIndex] = NULL;
	fifoHeadIndex = (fifoHeadIndex + 1) % FIFO_QUEUE_SIZE;
	pthread_mutex_unlock(&fifoMutex);

	return output;
}

// args is a pointer to song_t
static void* downloadThread()
{
	// Keep downloading as long as there are songs in the queue
	song_t* song = dequeueSong();
	while (song) {
		if (strcmp(song->filepath, "") != 0) {
                        printf(PRINTF_MODULE "Warning: Song does not have an empty file path, skipping\n");
                        song = dequeueSong();
                        continue;
                }

		// Run youtube-dl to download youtube audio as .wav file
		char cmdline[CMDLINE_MAX_LEN];
		sprintf(cmdline, DOWNLOAD_CMDLINE, song->vid);

		system(cmdline);

		// Update song with .wav filepath
		strcat(song->filepath, "/root/cache/");
		strcat(song->filepath, song->vid);
		strcat(song->filepath, WAV_EXT);

		// Update song status
                int status = control_setSongStatus(song, CONTROL_SONG_STATUS_LOADED);

                // Remove song if the status was set to removed
                if (status == CONTROL_SONG_STATUS_REMOVED) {
                        // TODO: remove song file if it isn't duplicated in the queue
                        // should probably call control_removeSong
                }

		song = dequeueSong();
	}

        pthread_mutex_unlock(&isDownloadingMutex);

	return 0;
}
