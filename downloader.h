#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include "control.h"

void downloader_init(void);
void downloader_cleanup(void);

void downloader_queueDownloadSong(song_t* song);
void downloader_deleteSongFile(song_t* song);

#endif
