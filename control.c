#include <stdio.h>

#include "control.h"

/*
 * Public functions
 */
int control_init(void)
{
        // TODO
        printf("Notice (control.c): control_init() called\n");
        (void)fflush(stdout);
        return 0;
}

void control_cleanup(void)
{
        // TODO
        printf("Notice (control.c): control_cleanup() called\n");
        (void)fflush(stdout);
}

void control_setMasterMode(void)
{
        // TODO
        printf("Notice (control.c): control_setMasterMode() called\n");
        (void)fflush(stdout);
}

void control_setSlaveMode(struct sockaddr_in sa)
{
        // TODO
        printf("Notice (control.c): control_setSlaveMode() called\n");
        (void)fflush(stdout);
}

enum control_mode control_getMode(void)
{
        // TODO
        printf("Notice (control.c): control_getMode() called\n");
        (void)fflush(stdout);
        return CONTROL_MODE_MASTER;
}

void control_queueAudio(char *buf, unsigned int length)
{
        // TODO
        (void)buf;
        (void)length;
        printf("Notice (control.c): control_queueAudio() called\n");
        (void)fflush(stdout);
}

void control_playAudio(void)
{
        // TODO
        printf("Notice (control.c): control_playAudio() called\n");
        (void)fflush(stdout);
}

void control_pauseAudio(void)
{
        // TODO
        printf("Notice (control.c): control_pauseAudio() called\n");
        (void)fflush(stdout);
}

void control_skipSong(void)
{
        // TODO
        printf("Notice (control.c): control_skipSong() called\n");
        (void)fflush(stdout);
}

void control_setRepeatStatus(int repeat)
{
        // TODO
        printf("Notice (control.c): control_setRepeatStatus() got %d\n", repeat);
        (void)fflush(stdout);
}

int control_getRepeatStatus(void)
{
        // TODO
        printf("Notice (control.c): control_getRepeatMode() called\n");
        (void)fflush(stdout);
        return 1;
}

void control_addSong(char *url)
{
        // TODO
        printf("Notice (control.c): control_addSong() got %s\n", url);
        (void)fflush(stdout);
}

void control_removeSong(char *url)
{
        // TODO
        printf("Notice (control.c): control_removeSong() got %s\n", url);
        (void)fflush(stdout);
}

const song_t *control_getQueue(void)
{
        // TODO
        printf("Notice (control.c): control_getQueue() called\n");
        (void)fflush(stdout);
        return NULL;
}

const song_t *control_getNextSong(void)
{
        // TODO
        printf("Notice (control.c): control_getNextSong() called\n");
        (void)fflush(stdout);
        return NULL;
}

void control_setVolume(unsigned int vol)
{
        // TODO
        printf("Notice (control.c): control_setVolume() got %u\n", vol);
        (void)fflush(stdout);
}

unsigned int control_getVolume(void)
{
        // TODO
        printf("Notice (control.c): control_getVolume() called\n");
        (void)fflush(stdout);
        return 80;
}

void control_addSlave(struct sockaddr_in addr)
{
        // TODO
        printf("Notice (control.c): control_addSlave() called\n");
        (void)fflush(stdout);
}

void control_verifySlaveStatus(struct sockaddr_in addr)
{
        // TODO
        printf("Notice (control.c): control_verifySlaveStatus() called\n");
        (void)fflush(stdout);
}
