//
// on target: apt install lame
//

#include "mp3towav.h"

int convert(char* filename, char* fileout)
{
  // if(argc != 2){
  //   printf("Enter the path of one mp3 file.\n");
  //   return 1;
  // }

  char command[MAX_LENGTH];
  printf("Converting file test1.mp3...\n");
  sprintf(command,"lame --decode %s %s",filename,fileout);
  system(command);
  printf("Finished converting,\n");

  return 0;
}
