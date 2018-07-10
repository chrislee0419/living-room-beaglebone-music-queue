#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "network.h"
#include "control.h"

#define INBOUND_PORT            12345
#define OUTBOUND_PORT           23456
#define BUFFER_SIZE             NETWORK_MAX_BUFFER_SIZE

#define CMD_OFFSET              4
#define CMD_SLAVE_OFFSET        11
// roughly the size of "vol=%d\nrepeat=%d\nqueue=" string and 12 commas
#define QUEUE_OFFSET            40

#define CMD_VOLUME_UP                   "volup"
#define CMD_VOLUME_DOWN                 "voldown"
#define CMD_PLAY                        "play"
#define CMD_PAUSE                       "pause"
#define CMD_SKIP                        "skip"
#define CMD_ADD_SONG                    "addsong=%s"
#define CMD_REMOVE_SONG                 "rmsong=%s"
#define CMD_REPEAT_SONG                 "repeat=%d"
#define CMD_CHANGE_MODE                 "mode="
#define CMD_SSCANF_MATCHES              1

#define VOL_DIFF                5

static int loop = 0;
static pthread_t th_rx;
static pthread_t th_tx;

struct out_msg {
        char buf[BUFFER_SIZE];
        struct sockaddr_in out_addr;
        struct out_msg *next;
};

static struct out_msg *out_queue = NULL;
static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t queue_sem;

/**
 * Helper functions
 */
static int getSocketFD(unsigned short port, int *fd)
{
        struct sockaddr_in sa;

        *fd = socket(PF_INET, SOCK_DGRAM, 0);
        if (*fd < 0) {
                printf("Error (network.c): unable to get file descriptor from socket()\n");
                (void)fflush(stdout);
                return errno;
        }

        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(*fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
                printf("Error (network.c): unable to bind to port\n");
                (void)fflush(stdout);
                return errno;
        }

        return 0;
}

static void queueOutboundMessage(char *buf, unsigned int buf_size, struct sockaddr_in sa)
{
        struct out_msg *msg, *elem;

        msg = malloc(sizeof(struct out_msg));
        if (!msg)
                goto out;

        (void)memset(msg, 0, sizeof(struct out_msg));
        (void)strncpy(msg->buf, buf, buf_size);
        msg->out_addr = sa;

        pthread_mutex_lock(&queue_mtx);
        {
                if (out_queue) {
                        elem = out_queue;
                        while (elem->next)
                                elem = elem->next;
                        elem->next = msg;
                } else {
                        out_queue = msg;
                }

                if (sem_post(&queue_sem) < 0) {
                        printf("Warning (network.c): semaphore is full, a message may be lost\n");
                        (void)fflush(stdout);
                }
        }
        pthread_mutex_unlock(&queue_mtx);

        return;
out:
        printf("Warning (network.c): unable to allocate memory for outbound message\n");
        (void)fflush(stdout);
}

static int queueSystemStatusMessage(struct sockaddr_in sa)
{
        char buf[BUFFER_SIZE] = {0};
        char *c;
        size_t bytes;
        const song_t *s;

        c = buf;
        s = control_getQueue();

        bytes = sprintf(c, "vol=%d\n", control_getVolume());
        if (!bytes)
                goto out;
        c += bytes;

        bytes = sprintf(c, "repeat=%d\n", control_getRepeatStatus());
        if (!bytes)
                goto out;
        c += bytes;

        bytes = sprintf(c, "queue=");
        if (!bytes)
                goto out;
        c += bytes;

        while (s) {
                if ((c-buf) >= BUFFER_SIZE - CONTROL_MAXLEN_URL - QUEUE_OFFSET) {
                        queueOutboundMessage(buf, (c-buf), sa);
                        (void)memset(buf, 0, BUFFER_SIZE);
                        c = buf;

                        bytes = sprintf(c, "queue=");
                        if (!bytes)
                                goto out;
                        c += bytes;
                }

                bytes = sprintf(c, "%s,", s->url);
                if (!bytes)
                        goto out;
                c += bytes;
                s = s->next;
        }

        queueOutboundMessage(buf, (c-buf), sa);

        return 0;
out:
        printf("Warning (network.c): failed to build system status string\n");
        (void)fflush(stdout);
        return errno;
}

static int processCmd(char *buf)
{
        char url[CONTROL_MAXLEN_URL] = {0};
        int num = 0;

        if (strstr(buf, CMD_VOLUME_UP)) {
                control_setVolume(control_getVolume() + VOL_DIFF);
        } else if (strstr(buf, CMD_VOLUME_DOWN)) {
                control_setVolume(control_getVolume() - VOL_DIFF);
        } else if (strstr(buf, CMD_PLAY)) {
                control_playAudio();
        } else if (strstr(buf, CMD_PAUSE)) {
                control_pauseAudio();
        } else if (strstr(buf, CMD_SKIP)) {
                control_skipSong();
        } else if (sscanf(buf, CMD_ADD_SONG, url) == CMD_SSCANF_MATCHES) {
                control_addSong(url);
        } else if (sscanf(buf, CMD_REMOVE_SONG, url) == CMD_SSCANF_MATCHES) {
                control_removeSong(url);
        } else if (sscanf(buf, CMD_REPEAT_SONG, &num) == CMD_SSCANF_MATCHES) {
                control_setRepeatStatus(num);
        } else if (strstr(buf, CMD_CHANGE_MODE)) {
                if (strstr(buf, "master")) {
                        control_setMasterMode();
                } else if (strstr(buf, "slave,")) {
                        struct sockaddr_in sa;
                        unsigned short port;
                        char *c;

                        buf += CMD_SLAVE_OFFSET;

                        if ((c = strchr(buf, ':'))) {
                                *c = '\0';
                                ++c;

                                port = atoi(c);

                                if (!port)
                                        port = INBOUND_PORT;
                        } else {
                                // try using INBOUND_PORT as default
                                port = INBOUND_PORT;
                        }

                        memset(&sa, 0, sizeof(sa));
                        sa.sin_family = AF_INET;
                        sa.sin_port = htons(port);
                        sa.sin_addr.s_addr = inet_addr(buf);
                        if (sa.sin_addr.s_addr < 0) {
                                printf("Warning (network.c): invalid address provided for master device\n");
                                (void)fflush(stdout);
                                return EADDRNOTAVAIL;
                        }

                        control_setSlaveMode(sa);
                }
        } else {
                printf("Warning (network.c): invalid command received (\"%s\")\n", buf);
                (void)fflush(stdout);
                return EINVAL;
        }

        return 0;
}

static void processMessage(char *buf, struct sockaddr_in sa)
{
        if (strstr(buf, "statusping\n")) {
                // web ui ping - send system status
                (void)queueSystemStatusMessage(sa);
        } else if (strstr(buf, "ping\n")) {
                // master/slave ping - used check connection
                unsigned int mode;

                mode = control_getMode();

                if (mode == CONTROL_MODE_MASTER) {
                } else if (mode == CONTROL_MODE_SLAVE) {
                        queueOutboundMessage("ping", strlen("ping")+1, sa);
                }
        } else if (strstr(buf, "audio\n")) {
                // audio data from master device
        } else if (strstr(buf, "cmd\n")) {
                char *cmd, *c;
                int err = 0;

                cmd = buf + CMD_OFFSET;
                c = cmd;

                printf("Notice (network.c): command received:\n\"%s\"\n", cmd);
                (void)fflush(stdout);

                while (*c != '\0' && (c-buf) < BUFFER_SIZE) {
                        if (*c == '\n' || (c-buf) >= BUFFER_SIZE) {
                                *c = '\0';
                                if ((err = processCmd(cmd)))
                                        break;
                                ++c;
                                cmd = c;
                        } else {
                                ++c;
                        }
                }

                if (err == EADDRNOTAVAIL) {
                        queueOutboundMessage("error=\"invalid address provided\"\n",
                                strlen("error=\"invalid address provided\"\n") + 1, sa);
                } else {
                        (void)queueSystemStatusMessage(sa);
                }
        } else {
                printf("Warning (network.c): invalid message received\n");
                (void)fflush(stdout);
                queueOutboundMessage("error=\"invalid message\"\n",
                        strlen("error=\"invalid message\"\n") + 1, sa);
        }

        return;
}

static void *receiverLoop(void *arg)
{
        int fd = 0;
        struct sockaddr_in sa;
        char buf[BUFFER_SIZE] = {0};
        int bytes_recv;
        unsigned int sa_len;

        if (getSocketFD(INBOUND_PORT, &fd))
                return NULL;

        while (loop) {
                sa_len = sizeof(sa);
                bytes_recv = recvfrom(fd, buf, BUFFER_SIZE-1, 0,
                        (struct sockaddr *)&sa, &sa_len);

                if (bytes_recv < 0) {
                        printf("Error (network.c): recvfrom encountered an error\n");
                        (void)fflush(stdout);
                        return NULL;
                }

                processMessage(buf, sa);
                (void)memset(buf, 0, BUFFER_SIZE);
        }

        return NULL;
}

static void *senderLoop(void *arg)
{
        int fd = 0;
        struct out_msg *msg;

        if (getSocketFD(OUTBOUND_PORT, &fd))
                return NULL;

        while (loop) {
                if (sem_wait(&queue_sem) < 0)
                        continue;
                if (!out_queue)
                        continue;

                pthread_mutex_lock(&queue_mtx);
                {
                        msg = out_queue;

                        if (sendto(fd, msg->buf, strlen(msg->buf), 0,
                            (struct sockaddr *)&(msg->out_addr), sizeof(msg->out_addr)) < 0) {
                                printf("Warning (network.c): sendto failed (%s)\n", strerror(errno));
                                (void)fflush(stdout);
                        }

                        out_queue = msg->next;
                        free(msg);
                        msg = NULL;
                }
                pthread_mutex_unlock(&queue_mtx);
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

        (void)sem_init(&queue_sem, 0, 0);

        if ((err = pthread_create(&th_rx, NULL, receiverLoop, NULL)))
                printf("Error (network.c): unable to create thread for incoming UDP connections\n");
        else if ((err = pthread_create(&th_tx, NULL, senderLoop, NULL)))
                printf("Error (network.c): unable to create thread for outgoing UDP connections\n");

        return err;
}

void network_cleanup(void)
{
        char buf[BUFFER_SIZE];

        loop = 0;

        // send a blank message to allow receiverLoop to exit recvfrom()
        // NOTE: Linux-specific
        (void)sprintf(buf, "echo \"a\" > /dev/udp/localhost/%d", INBOUND_PORT);
        if (system(buf)) {
                printf("Error (network.c): unable to send blank message to close app, close manually?\n");
                (void)fflush(stdout);
        }

        (void)pthread_join(th_rx, NULL);
        (void)pthread_join(th_tx, NULL);
}

void network_sendPlayCmd(void)
{
        // TODO
        printf("Notice (network.c): network_sendPlayCmd() called\n");
        (void)fflush(stdout);
}

void network_sendPauseCmd(void)
{
        // TODO
        printf("Notice (network.c): network_sendPauseCmd() called\n");
        (void)fflush(stdout);
}

void network_sendSkipCmd(void)
{
        // TODO
        printf("Notice (network.c): network_sendSkipCmd() called\n");
        (void)fflush(stdout);
}

void network_sendAudio(char *buf, int len)
{
        // TODO
        printf("Notice (network.c): network_sendAudio() called\n");
        (void)fflush(stdout);
        (void)buf;
        (void)len;
}
