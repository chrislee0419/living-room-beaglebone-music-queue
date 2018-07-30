#include "audio.h"

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alloca.h>
#include <pthread.h>
#include <errno.h>

#define PRINTF_MODULE   "[audio   ] "

#define DEFAULT_VOLUME  80
#define NUM_CHANNELS    2
#define SAMPLE_RATE     44100
#define BUFFER_SIZE_US  5e4

static snd_pcm_t *handle;
static int volume = 0;
static unsigned long buf_frames = 0;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/*
 * Helper functions
 */

/*
 * Public functions
 */
int audio_init(void)
{
        int err;
        unsigned long unused = 0;

        audio_setVolume(DEFAULT_VOLUME);

        err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (err)
                goto out;

        err = snd_pcm_set_params(handle,
                        SND_PCM_FORMAT_S16_LE,
                        SND_PCM_ACCESS_RW_INTERLEAVED,
                        NUM_CHANNELS,
                        SAMPLE_RATE,
                        1,
                        BUFFER_SIZE_US);
        if (err)
                goto out;

        err = snd_pcm_get_params(handle, &unused, &buf_frames);
out:
        if (err) {
                printf(PRINTF_MODULE "Error: could not initialize module\n");
                (void)fflush(stdout);
        }

        return err;
}

void audio_cleanup(void)
{
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
}

unsigned int audio_playAudio(short *buf, unsigned int size)
{
        snd_pcm_sframes_t frames;
	unsigned int frames_available;

	frames_available = size / NUM_CHANNELS;

        // clamp size parameter to buf_frames if necessary
        frames_available = frames_available <= buf_frames ? frames_available : buf_frames;

        // check if device is in a runnable state
        snd_pcm_state_t state = snd_pcm_state(handle);
        if (state != SND_PCM_STATE_PREPARED &&
            state != SND_PCM_STATE_RUNNING) {
                int err;

                err = snd_pcm_prepare(handle);
                state = snd_pcm_state(handle);

                if ((state != SND_PCM_STATE_PREPARED &&
                     state != SND_PCM_STATE_RUNNING) ||
                    err) {
                        printf(PRINTF_MODULE "Warning: audio handle is not in prepared or running state\n");
                        (void)fflush(stdout);
                        return 0;
                }
        }

        pthread_mutex_lock(&mtx);
        frames = snd_pcm_writei(handle, buf, frames_available);
        if (frames < 0) {
                printf(PRINTF_MODULE "Warning: snd_pcm_writei returned %li\n", frames);
                (void)fflush(stdout);

                frames = snd_pcm_recover(handle, frames, 1);
                if (frames < 0) {
                        printf(PRINTF_MODULE "Error: failed to write audio data with snd_pcm_writei\n");
                        (void)fflush(stdout);
                        return frames;
                }
        }

        pthread_mutex_unlock(&mtx);

        return frames * NUM_CHANNELS;
}

void audio_stopAudio(void)
{
        snd_pcm_drain(handle);
}

void audio_setVolume(unsigned int vol)
{
        long min, max;
        snd_mixer_t *hdl;
        snd_mixer_selem_id_t *sid;
        snd_mixer_elem_t *elem;

        if (vol < AUDIO_VOLUME_MIN)
                vol = AUDIO_VOLUME_MIN;
        else if (vol > AUDIO_VOLUME_MAX)
                vol = AUDIO_VOLUME_MAX;
        volume = vol;

        snd_mixer_open(&hdl, 0);
        snd_mixer_attach(hdl, "default");
        snd_mixer_selem_register(hdl, NULL, NULL);
        snd_mixer_load(hdl);

        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_index(sid, 0);
#ifdef MP_DESKTOP
        snd_mixer_selem_id_set_name(sid, "Master");
#else
        snd_mixer_selem_id_set_name(sid, "PCM");
#endif
        elem = snd_mixer_find_selem(hdl, sid);

        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        snd_mixer_selem_set_playback_volume_all(elem, (vol * max) / 100 + min);

        snd_mixer_close(hdl);
}

unsigned int audio_getVolume(void)
{
        return volume;
}
