
#include <alsa/asoundlib.h>

#define SAMPLE_RATE   44100
#define NUM_CHANNELS  1
#define SAMPLE_SIZE   (sizeof(short)) 	// bytes per sample

#define OUTFILE "audiofiles/out.wav"

typedef struct {
	int numSamples;
	short *pData;
} wavedata_t;

// Prototypes:
snd_pcm_t *Audio_openDevice();
void Audio_readWaveFileIntoMemory(char *fileName, wavedata_t *pWaveStruct);
void Audio_playFile(snd_pcm_t *handle, wavedata_t *pWaveData);



int playwav(void);
