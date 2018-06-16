#include <pthread.h>
#include <stdio.h>

#include "main.h"

static pthread_mutex_t mainMutex = PTHREAD_MUTEX_INITIALIZER;

static void initializeModules()
{
	printf("Initializing modules\n");

	// TODO: Web interface

	// TODO: Distributed networking setup

	// TODO: Music player
}

static void shutdownModules()
{
	printf("Shutting down modules\n");
}

int main()
{
	initializeModules();

	// mutex block after initialize, wait for shutdown to unlock
	pthread_mutex_init(&mainMutex, NULL);
	pthread_mutex_lock(&mainMutex);
	pthread_mutex_lock(&mainMutex);

 	shutdownModules();

	return 0;
}

void triggerShutdown() {
	pthread_mutex_unlock(&mainMutex);
	pthread_mutex_unlock(&mainMutex);
}
