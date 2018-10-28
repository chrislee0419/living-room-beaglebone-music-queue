#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <stdint.h>

#define AUDIO_VOLUME_MIN        0
#define AUDIO_VOLUME_MAX        100

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
 * Queue audio data to be played
 * @param buf Buffer containing audio data
 * @param size Size of buffer
 * @return Number of indices played; <0, error
 */
unsigned int audio_playAudio(int32_t *buf, unsigned int size);

/**
 * Flushes audio data buffer
 */
void audio_stopAudio(void);

/**
 * Set the volume for audio playback
 * @param vol Volume level from 0 to 100
 */
void audio_setVolume(unsigned int vol);

/**
 * Get the current volume level of the device
 * @return Volume level from 0 to 100
 */
unsigned int audio_getVolume(void);

#endif
