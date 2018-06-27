#ifndef _CONTROL_H_
#define _CONTROL_H_

#define CONTROL_MAXLEN_FN       256
#define CONTROL_MAXLEN_FN       128

enum control_mode {
        CONTROL_MODE_UNKNOWN = -1,
        CONTROL_MODE_MASTER,
        CONTROL_MODE_SLAVE
};

typedef struct {
        char fn[CONTROL_MAXLEN_FN];
        char url[CONTROL_MAXLEN_URL];
        int repeat;
        song_t *prev;
        song_t *next;
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
 * Set the BBG to be in master or slave mode.
 * @param ctrl Control mode
 */
void control_setMode(enum control_mode ctrl);

/**
 * Queue audio data to be played
 * @param buf Buffer containing audio data
 * @param length Length of buffer
 */
void control_playAudio(char *buf, unsigned int length);

/**
 * Pause audio
 */
void control_pauseAudio(void);

/**
 * Skip the current song being played
 */
void control_skipSong(void);

/**
 * Set the current song to be repeated
 */
void control_setRepeatSong(void);

/**
 * Add a song from YouTube to the queue
 * @param url YouTube URL
 */
void control_addSong(char *url);

/**
 * Remove a song from the queue
 * @param index Index of the song the be removed (0-indexed)
 */
void control_removeSong(unsigned int index);

/**
 * Produce a string containing the queue
 * @return Linked list of song_t objects
 */
song_t *control_getQueue(void);

/**
 * Get the next song in the queue
 * @return Address to the next song_t object (immutable)
 */
const song_t *control_getNextSong(void);

/**
 * Set the volume for audio playback
 * @param vol Volume level from 0 to 99
 */
void control_setVolume(unsigned int vol);

/**
 * Get the current volume level of the device
 * @return Volume level (0-99)
 */
int control_getVolume(void);

#endif
