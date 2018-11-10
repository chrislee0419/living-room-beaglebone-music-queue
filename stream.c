#include "stream.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PRINTF_MODULE           "[stream  ] "

#define DEFAULT_BUFFER_SIZE     4000
#define MAX_BUFFER_SIZE         1000000         // 1 MB

struct stream {
        CURLM *cmh;                     // curl multi handle
        CURL *ceh;                      // curl easy handle
        char *buf;
        unsigned int buf_used_size;
        unsigned int buf_alloc_size;
};

/**
 * Helper functions
 */
size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
        // NOTE: size is guaranteed to be 1, according to the libcurl doc
        // maximum size of nmemb is CURL_MAX_WRITE_SIZE (16K is default)
        stream_t *st;

        st = (stream_t *)userp;

        // resize the buffer if it isn't big enough
        if (st->buf_alloc_size - st->buf_used_size < nmemb) {
                char *temp;
                unsigned int new_size;

                // find out what size buffer is needed to store all the data
                new_size = ((nmemb - (st->buf_alloc_size - st->buf_used_size)) / DEFAULT_BUFFER_SIZE + 1) * DEFAULT_BUFFER_SIZE;

                // clamp to maximum size if necessary
                if (new_size > MAX_BUFFER_SIZE) {
                        new_size = MAX_BUFFER_SIZE;
                        nmemb = MAX_BUFFER_SIZE - st->buf_used_size;

                        printf(PRINTF_MODULE "Warning: maximum buffer size reached for stream object, libcurl will probably abort teh transfer now\n");
                        (void)fflush(stdout);
                }

                // attempt to reallocate memory for the buffer
                temp = realloc(st->buf, st->buf_alloc_size + DEFAULT_BUFFER_SIZE);
                if (!temp) {
                        printf(PRINTF_MODULE "Warning: unable to resize buffer for stream object, libcurl will probably abort the transfer now\n");
                        (void)fflush(stdout);

                        nmemb = st->buf_alloc_size - st->buf_used_size;
                } else {
                        st->buf_alloc_size = new_size;
                }
        }

        // copy received data to buffer
        (void)memcpy(st->buf + st->buf_used_size, buffer, nmemb);

        return nmemb;
}

unsigned int copy_to_user_buffer(stream_t *st, char *buf, unsigned int bytes)
{
        if (st->buf_used_size > bytes) {
                (void)memcpy(buf, st->buf, bytes);

                // realign data
                (void)memcpy(st->buf, st->buf + bytes,
                        st->buf_used_size - bytes);

                st->buf_used_size -= bytes;
                bytes = 0;
        } else {
                (void)memcpy(buf, st->buf, st->buf_used_size);
                bytes -= st->buf_used_size;
                st->buf_used_size = 0;
        }

        // number of bytes that are still available in the arg "bytes"
        return bytes;
}

/**
 * Public functions
 */
stream_t *stream_init(char *url)
{
        stream_t *st;

        if (!url)
                return NULL;

        st = malloc(sizeof(stream_t));
        if (!st) {
                printf(PRINTF_MODULE "Error: unable to allocate memory for stream object\n");
                (void)fflush(stdout);
                return NULL;
        }
        (void)memset(st, 0, sizeof(stream_t));

        st->buf = malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
        if (!st->buf) {
                printf(PRINTF_MODULE "Error: unable to allocate memory for buffer\n");
                (void)fflush(stdout);
                goto err1;
        }
        (void)memset(st->buf, 0, sizeof(char) * DEFAULT_BUFFER_SIZE);
        st->buf_alloc_size = DEFAULT_BUFFER_SIZE;

        // initialize curl multi handle to allow for async connections
        st->cmh = curl_multi_init();
        if (!st->cmh) {
                printf(PRINTF_MODULE "Error: unable to create curl multi handle\n");
                (void)fflush(stdout);
                goto err2;
        }

        // initialize one curl easy handle for stream connection
        st->ceh = curl_easy_init();
        if (!st->ceh) {
                printf(PRINTF_MODULE "Error: unable to create curl easy handle\n");
                (void)fflush(stdout);
                goto err3;
        }

        // configure easy handle
        (void)curl_easy_setopt(st->ceh, CURLOPT_URL, url);
        (void)curl_easy_setopt(st->ceh, CURLOPT_POST, 1);
        (void)curl_easy_setopt(st->ceh, CURLOPT_WRITEFUNCTION, write_data);
        (void)curl_easy_setopt(st->ceh, CURLOPT_WRITEDATA, st);

        (void)curl_multi_add_handle(st->cmh, st->ceh);

        printf(PRINTF_MODULE "Notice: stream object initialized for url: %s\n", url);
        (void)fflush(stdout);

        return st;

err3:
        (void)curl_multi_cleanup(st->cmh);
err2:
        free(st->buf);
err1:
        free(st);
        return NULL;
}

enum stream_status stream_pull_data(stream_t *st, char *buf, unsigned int *bytes)
{
        int running = 0;
        unsigned int bytes_to_copy = *bytes;
        CURLMcode res;

        if (!buf || bytes_to_copy <=  0)
                return STREAM_STATUS_FAIL;

        // copy whatever data was previously downloaded
        if (st->buf_used_size)
                bytes_to_copy = copy_to_user_buffer(st, buf, bytes_to_copy);

        // copy data from stream to intermediate buffer in stream_t object
        if ((res = curl_multi_perform(st->cmh, &running)) != CURLM_OK) {
                printf(PRINTF_MODULE "Warning: possible issue with curl while trying to stream (%s)\n", curl_multi_strerror(res));
                (void)fflush(stdout);
        }

        // copy newly acquired data if necessary
        if (bytes_to_copy > 0 && st->buf_used_size > 0)
                bytes_to_copy = copy_to_user_buffer(st, buf, bytes_to_copy);

        // no handles are running
        if (running == 0)  {
                struct CURLMsg *msg;
                enum stream_status status = STREAM_STATUS_COMPLETE;

                while ((msg = curl_multi_info_read(st->cmh, NULL))) {
                        if (msg->msg == CURLMSG_DONE) {
                                printf(PRINTF_MODULE "Notice: stream finished (%s)\n",
                                        curl_easy_strerror(msg->data.result));

                                if (status != STREAM_STATUS_FAIL && msg->data.result == CURLE_OK)
                                        status = STREAM_STATUS_COMPLETE;
                        } else {
                                printf(PRINTF_MODULE "Warning: possible issue at the end of stream (%p, %s)\n",
                                        msg->data.whatever, curl_easy_strerror(msg->data.result));

                                status = STREAM_STATUS_FAIL;
                        }
                        (void)fflush(stdout);
                }

                // return STREAM_STATUS_OK if there is still data available to be copied
                if (status == STREAM_STATUS_COMPLETE && st->buf_used_size)
                        return STREAM_STATUS_OK;
                else
                        return status;
        }

        if (bytes_to_copy == *bytes) {
                return STREAM_STATUS_OK_NODATA;
        } else {
                *bytes -= bytes_to_copy;
                return STREAM_STATUS_OK;
        }
}

void stream_free(stream_t *st)
{
        if (st) {
                (void)curl_multi_remove_handle(st->cmh, st->ceh);
                curl_easy_cleanup(st->ceh);
                (void)curl_multi_cleanup(st->cmh);

                free(st->buf);
                free(st);
        }
}
