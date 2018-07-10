#include "audio.h"

int main(int argc, char* argv[])
{
  printf("testing.\n");
  char* infile = "audiofiles/test1.mp3";
  char* outfile = "audiofiles/out.wav";
  convert(infile, outfile);
}
