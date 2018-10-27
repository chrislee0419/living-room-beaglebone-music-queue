#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <netinet/in.h>

#define CONTROL_MAXLEN_FN               256
#define CONTROL_MAXLEN_VID              16

#define CONTROL_RMSONG_FIRST            -1
#define CONTROL_PREV_SONGS_LIST_LEN     2

enum control_song_status {
        CONTROL_SONG_STATUS_UNKNOWN     = -1,
        CONTROL_SONG_STATUS_QUEUED      = 0,    // song is not ready to be played
        CONTROL_SONG_STATUS_LOADED      = 1,    // ready to play
        CONTROL_SONG_STATUS_REMOVED     = 2,    // marked for removal
};

typedef struct song {
        char filepath[CONTROL_MAXLEN_FN];       // filepath to wav data
        char vid[CONTROL_MAXLEN_VID];           // YouTube video ID
        enum control_song_status status;
        struct song *prev;                      // Previous song in the queue
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
 * @return 0, if not repeating; 1, if repeating
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
 * @param index Index of song in queue
 * @return 0, if successful; otherwise, error
 */
int control_removeSong(char *url, int index);

/**
 * Copies the path to the song's associated music file to a provided buffer.
 * @param song Song object
 * @param buf Address to a buffer
 * @return 0, if successful; otherwise, error
 */
int control_getSongFilepath(song_t *song, char **buf);

/**
 * Set the status of a song. Should be used for thread safety.
 * NOTE: The song will be deleted if the song's status was set to CONTROL_SONG_STATUS_REMOVED
 * @param song Address of song to change (must exist in the queue)
 * @param status Status to set for the song
 * @return Current status of the song; CONTROL_SONG_STATUS_UNKNOWN, if error
 */
enum control_song_status control_setSongStatus(song_t *song, enum control_song_status status);

/**
 * Get the playback progress of the current song
 * @param curr Address to store current position of playback
 * @param end Address to store total indices of song
 */
void control_getSongProgress(int *curr, int *end);

/**
 * Get the song queue. First item is the current song
 * @return Linked list of song_t objects
 */
const song_t *control_getQueue(void);

/**
 * Get the previous CONTROL_PREV_SONGS_LIST_LEN songs played
 * First song is the most recently played song
 * @return Linked list of song_t objects
 */
const song_t *control_getPlayedSongs(void);

/**
 * Callback when a download completes
 * @param song Address of song that finished downloading
 * @return 0, if successful; otherwise, error
 */
int control_onDownloadComplete(song_t* song);

#endif
