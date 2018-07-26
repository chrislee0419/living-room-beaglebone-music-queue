#include "downloader.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMDLINE_MAX_LEN 1024
static const char* DOWNLOAD_CMDLINE = "youtube-dl --extract-audio --audio-format wav -o '~/cache/%%(id)s.%%(ext)s' https://www.youtube.com/watch?v=%s";
static const char* RM_CACHE_CMDLINE = "rm ~/cache/*";
static const char* WAV_EXT = ".wav";

#define FIFO_QUEUE_SIZE 5
static song_t* songFifoQueue[FIFO_QUEUE_SIZE];
static int fifoHeadIndex = 0;
static int fifoTailIndex = 0;	// The next empty slot, at the end of the queue
static pthread_mutex_t fifoMutex = PTHREAD_MUTEX_INITIALIZER;

static bool isDownloading = false;


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
	if (!isDownloading) {
		pthread_t pDownloadThread;
		pthread_create(&pDownloadThread, 0, &downloadThread, 0);
	}
}


/*
 * Private functions
 */

static void enqueueSong(song_t* song) {
	if (songFifoQueue[fifoTailIndex] != NULL) {
		printf("Error (downloader.c): Download queue full - item not queued\n");
		return;
	}

	pthread_mutex_lock(&fifoMutex);
	songFifoQueue[fifoTailIndex] = song;
	pthread_mutex_unlock(&fifoMutex);

	fifoTailIndex = (fifoTailIndex + 1) % FIFO_QUEUE_SIZE;
}

static song_t* dequeueSong() {
	if (fifoHeadIndex == fifoTailIndex) {
		return NULL;
	}

	pthread_mutex_lock(&fifoMutex);
	song_t* output = songFifoQueue[fifoHeadIndex];
	songFifoQueue[fifoHeadIndex] = NULL;
	pthread_mutex_unlock(&fifoMutex);

	fifoHeadIndex = (fifoHeadIndex + 1) % FIFO_QUEUE_SIZE;

	return output;
}

// args is a pointer to song_t
static void* downloadThread()
{
	isDownloading = true;

	// Keep downloading as long as there are songs in the queue
	song_t* song = dequeueSong();
	while (song) {

		assert(song);
		assert(strcmp(song->filepath, "") == 0);

		// Run youtube-dl to download youtube audio as .wav file
		char cmdline[CMDLINE_MAX_LEN];
		sprintf(cmdline, DOWNLOAD_CMDLINE, song->vid);

		system(cmdline);

		// Update song with .wav filepath
		strcat(song->filepath, "cache/");
		strcat(song->filepath, song->vid);
		strcat(song->filepath, WAV_EXT);

		// Update song status
		song->status = CONTROL_SONG_STATUS_LOADED;

		song = dequeueSong();
	}

	isDownloading = false;

	return 0;
}
