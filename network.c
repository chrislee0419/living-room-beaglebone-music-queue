#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include <network.h>

#define INTERNAL_PORT 12345
#define BUFFER_SIZE 1500

static int loop = 0;
static pthread_t th_tcp;
static pthread_t th_udp;

/**
 * Helper functions
 */
static void processUDPAudio(char *buf, int buf_size)
{
        // UDP audio data from master device

}

static void processUDPMessage(char *buf, int buf_size)
{
        // UDP messages from the web server
}

static void *mainTCPLoop(void *arg)
{
        return NULL;
}

static void *mainUDPLoop(void *arg)
{
        int fd;
        struct sockaddr_in sa;
        char buf[BUFFER_SIZE] = {0};
        int bytes_recv;
        unsigned int sa_len;

        // initialize socket
        fd = socket(PF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
                printf("Error (network.c): unable to get file descriptor from socket()\n");
                (void)fflush(stdout);
                return NULL;
        }

        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(PORT);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
                printf("Error (network.c): unable to bind to port\n");
                (void)fflush(stdout);
                return NULL;
        }

        while (loop) {
                sa_len = sizeof(sa);
                bytes_recv = recvfrom(fd, buf, BUFFER_SIZE-1, 0,
                        (struct sockaddr *)&sa, &sa_len);

                if (bytes_recv < 0) {
                        printf("Error (network.c): recvfrom encountered an error\n");
                        (void)fflush(stdout);
                        return NULL;
                }

                processUDPMessage(buf, BUFFER_SIZE);
        }

        return NULL;
}

/**
 * Public functions
 */
int network_init(void)
{
        int err = 0;

        loop = 1;

        if ((err = pthread_create(&th_tcp, NULL, mainUDPLoop, NULL)))
                printf("Error (network.c): unable to create thread for UDP connections");

        return err;
}

void network_cleanup(void)
{
        loop = 0;
}

void network_sendPlayCmd(void)
{
}

void network_sendPauseCmd(void)
{
}

void network_sendSkipCmd(void)
{
}

