#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include "control.h"

void downloader_init(void);
void downloader_cleanup(void);

void downloader_downloadSong(song_t* song);

#endif
