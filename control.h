#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <netinet/in.h>

#define CONTROL_MAXLEN_FN       256
#define CONTROL_MAXLEN_URL      128

enum control_mode {
        CONTROL_MODE_UNKNOWN = -1,
        CONTROL_MODE_MASTER,
        CONTROL_MODE_SLAVE
};

enum control_song_status {
        CONTROL_SONG_STATUS_QUEUED,
        CONTROL_SONG_STATUS_LOADING,
        CONTROL_SONG_STATUS_LOADED,
        CONTROL_SONG_STATUS_REMOVED             // When song is removed while downloading
};

typedef struct song {
        char filepath[CONTROL_MAXLEN_FN];       // filepath to wav data
        char vid[CONTROL_MAXLEN_URL];           // YouTube video ID
        enum control_song_status status;
        struct song *next;                      // Next song in the queue
} song_t;

/**
 * Initializes this module
 * @return 0 if successful, otherwise error
 */
int control_init(void);

/**
 * Cleans up this module
 */
void control_cleanup(void);

/**
 * Set the music player to be in master mode
 */
void control_setMode(enum control_mode m);

/**
 * Gets the current mode set for the BBG
 * @return Control mode
 */
enum control_mode control_getMode(void);

/**
 * Queue audio data to be played
 * @param buf Buffer containing audio data
 * @param length Length of buffer
 */
void control_queueAudio(char *buf, unsigned int length);

/**
 * Resume playing audio
 */
void control_playAudio(void);

/**
 * Pause audio
 */
void control_pauseAudio(void);

/**
 * Get whether the device is playing audio
 * @return Play status
 */
int control_getPlayStatus(void);

/**
 * Skip the current song being played
 */
void control_skipSong(void);

/**
 * Set the current song to be repeated
 */
void control_setRepeatStatus(int repeat);

/**
 * Gets the status of whether the song is repeated
 * @return 0 if not repeating, 1 if repeating
 */
int control_getRepeatStatus(void);

/**
 * Add a song from YouTube to the queue
 * @param url YouTube URL of song
 */
void control_addSong(char *url);

/**
 * Remove a song from the queue
 * @param url YouTube URL of song in queue
 */
void control_removeSong(char *url);

/**
 * Produce a string containing the queue
 * @return Linked list of song_t objects
 */
const song_t *control_getQueue(void);

/**
 * Get the next song in the queue
 * @return Address to the next song_t object (immutable)
 */
const song_t *control_getNextSong(void);

/**
 * Register a slave device
 * @param addr Address associated with slave device
 */
void control_addSlave(struct sockaddr_in addr);

/**
 * To be called when the slave device successfully responds to a ping
 * @param addr Address associated with slave device
 */
void control_verifySlaveStatus(struct sockaddr_in addr);

/**
 * Callback when a download completes
 */
void control_onDownloadComplete(void);

#endif
