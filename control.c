#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "downloader.h"

#include "control.h"

#define NUM_SONGS_TO_DOWNLOAD 3

static song_t *song_list = NULL;


/*
 * Forward declations
 */
void updateDownloadedSongs();

void debugPrintSongList();

/*
 * Public functions
 */
int control_init(void)
{
        // TODO
        printf("Notice (control.c): control_init() called\n");
        (void)fflush(stdout);

        downloader_init();

        return 0;
}

void control_cleanup(void)
{
        downloader_cleanup();

        // TODO
        printf("Notice (control.c): control_cleanup() called\n");
        (void)fflush(stdout);

        song_t *current_song = song_list;
        song_t *next_song = NULL;
        while (current_song) {
                next_song = current_song->next;
                free(current_song);
                current_song = next_song;
        }
}

void control_setMasterMode(void)
{
        // TODO
        printf("Notice (control.c): control_setMasterMode() called\n");
        (void)fflush(stdout);
}

void control_setSlaveMode(struct sockaddr_in sa)
{
        // TODO
        printf("Notice (control.c): control_setSlaveMode() called\n");
        (void)fflush(stdout);
}

enum control_mode control_getMode(void)
{
        // TODO
        printf("Notice (control.c): control_getMode() called\n");
        (void)fflush(stdout);
        return CONTROL_MODE_MASTER;
}

void control_queueAudio(char *buf, unsigned int length)
{
        // TODO
        (void)buf;
        (void)length;
        printf("Notice (control.c): control_queueAudio() called\n");
        (void)fflush(stdout);
}

void control_playAudio(void)
{
        // TODO
        printf("Notice (control.c): control_playAudio() called\n");
        (void)fflush(stdout);
}

void control_pauseAudio(void)
{
        // TODO
        printf("Notice (control.c): control_pauseAudio() called\n");
        (void)fflush(stdout);
}

void control_skipSong(void)
{
        // TODO
        printf("Notice (control.c): control_skipSong() called\n");
        (void)fflush(stdout);
}

void control_setRepeatStatus(int repeat)
{
        // TODO
        printf("Notice (control.c): control_setRepeatStatus() got %d\n", repeat);
        (void)fflush(stdout);
}

int control_getRepeatStatus(void)
{
        // TODO
        // printf("Notice (control.c): control_getRepeatStatus() called\n");
        // (void)fflush(stdout);
        return 1;
}

void control_addSong(char *url)
{
        // TODO
        printf("Notice (control.c): control_addSong() got %s\n", url);
        (void)fflush(stdout);

        song_t* new_song = malloc(sizeof(song_t));

        strcpy(new_song->filepath, "");
        strcpy(new_song->vid, url);

        new_song->next = NULL;
        new_song->status = SONG_STATUS_QUEUED;

        // Add to end of list
        if (!song_list) {
                song_list = new_song;
        }
        else {
                song_t *current_song = song_list;
                while (current_song->next != NULL) {
                        current_song = current_song->next;
                }

                current_song->next = new_song;
        }

        // Download songs if needed
        updateDownloadedSongs();
        debugPrintSongList();
}

void control_removeSong(char *url)
{
        // TODO
        printf("Notice (control.c): control_removeSong() got %s\n", url);
        (void)fflush(stdout);

        // Find song in list



        // Delete file from disk


        // Download new songs if needed
        updateDownloadedSongs();
}

const song_t *control_getQueue(void)
{
        // TODO
        // printf("Notice (control.c): control_getQueue() called\n");
        // (void)fflush(stdout);
        return NULL;
}

const song_t *control_getNextSong(void)
{
        // TODO
        printf("Notice (control.c): control_getNextSong() called\n");
        (void)fflush(stdout);
        return NULL;
}

void control_setVolume(unsigned int vol)
{
        // TODO
        printf("Notice (control.c): control_setVolume() got %u\n", vol);
        (void)fflush(stdout);
}

unsigned int control_getVolume(void)
{
        // TODO
        // printf("Notice (control.c): control_getVolume() called\n");
        // (void)fflush(stdout);
        return 80;
}

void control_addSlave(struct sockaddr_in addr)
{
        // TODO
        printf("Notice (control.c): control_addSlave() called\n");
        (void)fflush(stdout);
}

void control_verifySlaveStatus(struct sockaddr_in addr)
{
        // TODO
        printf("Notice (control.c): control_verifySlaveStatus() called\n");
        (void)fflush(stdout);
}


/*
 * Private functions
 */

// Checks if any songs need to be downloaded and processed
// Downloads them in a new background
void updateDownloadedSongs()
{
        // Go through first N songs and download them if not already downloaded
        song_t* current_song = song_list;
        for (int i = 0; i < NUM_SONGS_TO_DOWNLOAD; i++) {
                if (!current_song) {
                        break;
                } 

                if (current_song->status == SONG_STATUS_LOADING
                        || current_song->status == SONG_STATUS_LOADED) {
                        // Do nothing
                }
                else if (current_song->status == SONG_STATUS_QUEUED) {
                        // Download the song in new thread
                        downloader_queueDownloadSong(current_song);

                        // Update song status
                        current_song->status = SONG_STATUS_LOADING;
                }
                else if (current_song->status == SONG_STATUS_REMOVED) {
                        // Tried to remove song while downloading
                        // Remove it here now
                        control_removeSong(current_song->vid);
                }

                current_song = current_song->next;
        }
}

void debugPrintSongList()
{
        song_t* current_song = song_list;
        while (current_song) {
                char statusStr[10];
                switch (current_song->status) {
                        case SONG_STATUS_QUEUED:
                                strcpy(statusStr, "QUEUED");
                                break;
                        case SONG_STATUS_LOADING:
                                strcpy(statusStr, "LOADING");
                                break;
                        case SONG_STATUS_LOADED:
                                strcpy(statusStr, "LOADED");
                                break;
                        case SONG_STATUS_REMOVED:
                                strcpy(statusStr, "REMOVED");
                                break;
                }
                printf("file=[%s], id=[%s], status=[%s]\n", current_song->filepath, current_song->vid, statusStr);
                current_song = current_song->next;
        }
}
