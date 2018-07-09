#ifndef _AUDIO_H_
#define _AUDIO_H_

/**
 * Initializes this module
 * @return 0 if successful, otherwise error
 */
int audio_init(void);

/**
 * Cleans up this module
 */
void audio_cleanup(void);

/**
 * Set the song to be played
 * @param fn Filename of the song (must be NULL-terminated correctly)
 */
void audio_setSong(char *fn);

/**
 * Resume playing the current song
 */
void audio_resume(void);

/**
 * Pause the song
 */
void audio_pause(void);

#endif
