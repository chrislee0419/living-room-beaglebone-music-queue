#include "main.h"

#include "control.h"
#include "network.h"
#include "audio.h"

#include <pthread.h>
#include <stdio.h>

#define PRINTF_MODULE   "[main    ] "

static pthread_mutex_t mainMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Helper methods
 */

static int initializeModules()
{
        int err = 0;

        printf(PRINTF_MODULE "Notice: Initializing modules\n");
        (void)fflush(stdout);

        err |= control_init();
        err |= network_init();
        err |= audio_init();

        return err;
}

static void shutdownModules()
{
        printf(PRINTF_MODULE "Notice: shutting down modules\n");
        (void)fflush(stdout);

        network_cleanup();
        control_cleanup();
        audio_cleanup();
}

int main()
{
        if (initializeModules()) {
                printf(PRINTF_MODULE "Error: unable to start program\n");
                (void)fflush(stdout);
                return 1;
        } else {
                printf(PRINTF_MODULE "Notice: modules initialized successfully\n");
                (void)fflush(stdout);
        }

        // wait for shutdown to unlock
        pthread_mutex_lock(&mainMutex);
        pthread_mutex_lock(&mainMutex);

        shutdownModules();

        return 0;
}

/*
 * Public methods
 */

void main_triggerShutdown() {
        printf(PRINTF_MODULE "Notice: triggering shutdown\n");
        (void)fflush(stdout);
        pthread_mutex_unlock(&mainMutex);
}
