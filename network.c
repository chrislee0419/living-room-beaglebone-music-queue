#include "network.h"

#include "control.h"
#include "audio.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#define PRINTF_MODULE           "[network ] "

#define INBOUND_PORT            12345
#define OUTBOUND_PORT           23456
#define BUFFER_SIZE             NETWORK_MAX_BUFFER_SIZE

#define CMD_MASTER_OFFSET       12
#define CMD_SLAVE_OFFSET        11
// assuming a youtube video id's minimum length is 10, this is roughly the size
// of a fully built status response string (minus the queue) with 145 commas
#define QUEUE_OFFSET            222

#define CMD_PING                        "ping"
#define CMD_VOLUME_UP                   "volup"
#define CMD_VOLUME_DOWN                 "voldown"
#define CMD_PLAY                        "play"
#define CMD_PAUSE                       "pause"
#define CMD_SKIP                        "skip"
#define CMD_ADD_SONG                    "addsong=%s"
#define CMD_ADD_SONG_SSCANF_MATCHES     1
#define CMD_REMOVE_SONG                 "rmsong=%s,%d"
#define CMD_REMOVE_SONG_SSCANF_MATCHES  2
#define CMD_REPEAT_SONG                 "repeat=%d"
#define CMD_REPEAT_SONG_SSCANF_MATCHES  1
#define CMD_SET_VOL                     "vol=%d"
#define CMD_SET_VOL_SSCANF_MATCHES      1

#define VOL_DIFF                        5

static int loop = 0;
static pthread_t th_rx;
static pthread_t th_tx;

struct out_msg {
        char buf[BUFFER_SIZE];
        int msg_len;
        struct sockaddr_in out_addr;
        struct out_msg *next;
};

static struct out_msg *out_queue = NULL;
static pthread_mutex_t mtx_queue = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem_queue;

/**
 * Helper functions
 */
static int getSocketFD(unsigned short port, int *fd)
{
        struct sockaddr_in sa;

        *fd = socket(PF_INET, SOCK_DGRAM, 0);
        if (*fd < 0) {
                printf(PRINTF_MODULE "Error: unable to get file descriptor from socket()\n");
                (void)fflush(stdout);
                return errno;
        }

        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(*fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
                printf(PRINTF_MODULE "Error: unable to bind to port\n");
                (void)fflush(stdout);
                return errno;
        }

        return 0;
}

static void queueOutboundMessage(char *buf, unsigned int buf_size, struct sockaddr_in sa)
{
        struct out_msg *msg, *elem;

        if (buf_size > BUFFER_SIZE) {
                printf(PRINTF_MODULE "Warning: outbound message is larger than maximum buffer size, not sending\n");
                (void)fflush(stdout);
                return;
        }

        msg = malloc(sizeof(struct out_msg));
        if (!msg) {
                printf(PRINTF_MODULE "Warning: unable to allocate memory for outbound message\n");
                (void)fflush(stdout);
                return;
        }

        (void)memset(msg, 0, sizeof(struct out_msg));
        (void)strncpy(msg->buf, buf, buf_size);
        msg->msg_len = buf_size;
        msg->out_addr = sa;

        pthread_mutex_lock(&mtx_queue);
        {
                if (out_queue) {
                        elem = out_queue;
                        while (elem->next)
                                elem = elem->next;
                        elem->next = msg;
                } else {
                        out_queue = msg;
                }

                if (sem_post(&sem_queue) < 0) {
                        printf(PRINTF_MODULE "Warning: semaphore is full, a message may be lost\n");
                        (void)fflush(stdout);
                }
        }
        pthread_mutex_unlock(&mtx_queue);
}

static int queueSystemStatusMessage(struct sockaddr_in sa)
{
        char buf[BUFFER_SIZE] = {0};
        char *c;
        char *vids;
        size_t bytes;
        int song_status;

        c = buf;
        vids = control_getQueueVIDs();

        bytes = sprintf(c, "vol=%d\n", audio_getVolume());
        if (!bytes)
                goto out;
        c += bytes;

        bytes = sprintf(c, "play=%d\n", control_getPlayStatus());
        if (!bytes)
                goto out;
        c += bytes;

        bytes = sprintf(c, "repeat=%d\n", control_getRepeatStatus());
        if (!bytes)
                goto out;
        c += bytes;

        song_status = s ? s->status : CONTROL_SONG_STATUS_UNKNOWN;
        bytes = sprintf(c, "status=%d\n", song_status);
        if (!bytes)
                goto out;
        c += bytes;

        // if there exists a song in the queue, we should be able to show
        // the play progress of the current song
        if (vids) {
                int play_curr;
                int play_end;

                control_getSongProgress(&play_curr, &play_end);
                bytes = sprintf(c, "progress=%d/%d\n", play_curr, play_end);
                if (!bytes)
                        goto out;
                c += bytes;
        }

        bytes = sprintf(c, "queue=");
        if (!bytes)
                goto out;
        c += bytes;

        if (vids) {
                char *pos = vids;

                // send in batches if entire string doesn't fit in the buffer
                while (strlen(pos) >= BUFFER_SIZE - (c-buf)) {
                        char *next_pos;

                        // place what we can into the buffer
                        next_pos = pos + BUFFER_SIZE - (c-buf) - CONTROL_MAXLEN_VID - 1;
                        bytes = 0;

                        // buffer has to be able to fit at least one of the video id strings
                        if (next_pos >= pos) {
                                // NOTE: strchr should never return NULL, otherwise we
                                // can fit the remaining chars in pos into the buffer
                                next_pos = strchr(next_pos, ',');

                                *next_pos = '\0';
                                ++next_pos;

                                (void)strcpy(c, pos);
                                bytes = next_pos - pos;
                        } else {
                                // unable to fit any video id strings
                                next_pos = pos;
                        }

                        queueOutboundMessage(buf, (c-buf) + bytes, sa);
                        (void)memset(buf, 0, BUFFER_SIZE);
                        c = buf;

                        // begin new outbound message that contains the rest of the queue
                        bytes = sprintf(c, "queuemore=");
                        if (!bytes) {
                                free(vids);
                                goto out;
                        }

                        c += bytes;
                        pos = next_pos;
                }

                // send remaining part of the string
                (void)strcpy(c, pos);
                c += strlen(pos);

                // finished with the string, free it now
                free(vids);
        }

        queueOutboundMessage(buf, (c-buf), sa);

        return 0;
out:
        printf(PRINTF_MODULE "Warning: failed to build system status string\n");
        (void)fflush(stdout);
        return errno;
}

static int processCmd(char *buf)
{
        char url[CONTROL_MAXLEN_VID] = {0};
        int num = 0;

        if (strstr(buf, CMD_PING)) {
                // do nothing, since a status message is returned on valid commands
        } else if (strstr(buf, CMD_VOLUME_UP)) {
                audio_setVolume(audio_getVolume() + VOL_DIFF);
        } else if (strstr(buf, CMD_VOLUME_DOWN)) {
                audio_setVolume(audio_getVolume() - VOL_DIFF);
        } else if (sscanf(buf, CMD_SET_VOL, &num) == CMD_SET_VOL_SSCANF_MATCHES) {
                audio_setVolume(num);
        } else if (strstr(buf, CMD_PLAY)) {
                control_playAudio();
        } else if (strstr(buf, CMD_PAUSE)) {
                control_pauseAudio();
        } else if (strstr(buf, CMD_SKIP)) {
                control_skipSong();
        } else if (sscanf(buf, CMD_ADD_SONG, url) == CMD_ADD_SONG_SSCANF_MATCHES) {
                control_addSong(url);
        } else if (sscanf(buf, CMD_REMOVE_SONG, url, &num) == CMD_REMOVE_SONG_SSCANF_MATCHES) {
                control_removeSong(url, num);
        } else if (sscanf(buf, CMD_REPEAT_SONG, &num) == CMD_REPEAT_SONG_SSCANF_MATCHES) {
                control_setRepeatStatus(num);
        } else {
                printf(PRINTF_MODULE "Warning: invalid command received (\"%s\")\n", buf);
                (void)fflush(stdout);
                return EINVAL;
        }

        return 0;
}

static void processMessage(char *buf, struct sockaddr_in sa)
{
        char *cmd, *c;
        int err = 0;

        cmd = buf;
        c = buf;

        while (*c != '\0' && (c-buf) < BUFFER_SIZE) {
                if (*c == '\n' || (c-buf) >= BUFFER_SIZE) {
                        *c = '\0';

                        if (!strstr(cmd, CMD_PING)) {
                                printf(PRINTF_MODULE "Notice: processing command: \"%s\"\n", cmd);
                                (void)fflush(stdout);
                        }

                        if ((err = processCmd(cmd)))
                                break;
                        ++c;
                        cmd = c;
                } else {
                        ++c;
                }
        }

        // error handling
        if (err == EINVAL) {
                queueOutboundMessage("error=\"invalid command\"\n",
                        strlen("error=\"invalid command\"\n") + 1, sa);
        } else {
                (void)queueSystemStatusMessage(sa);
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
                        printf(PRINTF_MODULE "Error: receiverLoop's recvfrom encountered an error\n");
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
                if (sem_wait(&sem_queue) < 0)
                        continue;
                if (!out_queue)
                        continue;

                pthread_mutex_lock(&mtx_queue);
                {
                        msg = out_queue;

                        if (sendto(fd, msg->buf, msg->msg_len, 0,
                            (struct sockaddr *)&(msg->out_addr), sizeof(msg->out_addr)) < 0) {
                                printf(PRINTF_MODULE "Warning: sendto failed (%s)\n", strerror(errno));
                                (void)fflush(stdout);
                        }

                        out_queue = msg->next;
                        free(msg);
                        msg = NULL;
                }
                pthread_mutex_unlock(&mtx_queue);
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

        (void)sem_init(&sem_queue, 0, 0);

        if ((err = pthread_create(&th_rx, NULL, receiverLoop, NULL)))
                printf(PRINTF_MODULE "Error: unable to create thread for incoming UDP connections\n");
        else if ((err = pthread_create(&th_tx, NULL, senderLoop, NULL)))
                printf(PRINTF_MODULE "Error: unable to create thread for outgoing UDP connections\n");

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
                printf(PRINTF_MODULE "Error: unable to send blank message to close app, close manually?\n");
                (void)fflush(stdout);
        }

        (void)pthread_join(th_rx, NULL);
        (void)pthread_join(th_tx, NULL);
}
