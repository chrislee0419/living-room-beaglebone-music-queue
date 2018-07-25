#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "downloader.h"

#define CMDLINE_MAX_LEN 1024
static const char* DOWNLOAD_CMDLINE = "youtube-dl --extract-audio --audio-format wav -o 'cache/%%(id)s.%%(ext)s' https://www.youtube.com/watch?v=%s";
static const char* RM_CACHE_CMDLINE = "rm cache/*";
static const char* WAV_EXT = ".wav";

/*
 * Forward declations
 */
static void* downloadThread(void *args);


/*
 * Public functions
 */
void downloader_init(void) 
{
	// Clear cache
	system(RM_CACHE_CMDLINE);
}

void downloader_cleanup(void) 
{
	// Clear cache
	system(RM_CACHE_CMDLINE);
}

void downloader_downloadSong(song_t* song) 
{
	// Spawn a thread to do everything
	pthread_t pDownloadThread;
	pthread_create(&pDownloadThread, 0, &downloadThread, song);
}


/*
 * Private functions
 */

// args is a pointer to song_t
static void* downloadThread(void *args)
{
	song_t* song = args;
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
	song->status = SONG_STATUS_LOADED;

	return 0;
}
