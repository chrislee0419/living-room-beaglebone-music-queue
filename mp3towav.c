//
// //requires lame
// //sudo apt-get install libmp3lame-dev
//
// v2:
// on target: apt install lame
//

#include <stdio.h>
#include <stdlib.h>
// #include <alsa/asoundlib.h>
#include <string.h>
//#include "lame/lame.h"

#define INFILE "audiofiles/test1.mp3"
#define OUTFILE "audiofiles/out.wav"
#define MAX_LENGTH 1000

int main(int argc, char* argv[])
{
  printf("testing\n");

  char command[MAX_LENGTH];

  // strcpy(command, "touch audiofiles/output.wav");
  strcpy(command, "lame --decode audiofiles/test1.mp3 audiofiles/output.wav");
  system(command);

  return 0;

  //
  // FILE *mp3 = fopen(INFILE, "rf");
  // FILE *wav = fopen(OUTFILE, "wf");
  //
  // lame_t lame = lame_init();
  // lame_set_in_samplerate(lame,44100);
  // lame_set_VBR(lame,vbr_default);
  // lame_init_params(lame);
  //
  // lame_close(lame);
  //
  // fclose(mp3);
  // fclose(wav);

}
