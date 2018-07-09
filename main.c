#include <pthread.h>
#include <stdio.h>

#include "main.h"
#include "network.h"

static pthread_mutex_t mainMutex = PTHREAD_MUTEX_INITIALIZER;

static int initializeModules()
{
        int err = 0;

	printf("Initializing modules\n");

	// TODO: Web interface

	// TODO: Distributed networking setup
        err |= network_init();

	// TODO: Music player

        return err;
}

static void shutdownModules()
{
	printf("Shutting down modules\n");

        network_cleanup();
}

int main()
{
	if (initializeModules()) {
                printf("Unable to start program\n");
                return 1;
        }

	// wait for shutdown to unlock
	pthread_mutex_lock(&mainMutex);
	pthread_mutex_lock(&mainMutex);

 	shutdownModules();

	return 0;
}

void triggerShutdown() {
	pthread_mutex_unlock(&mainMutex);
}
