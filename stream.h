#ifndef _STREAM_H_
#define _STREAM_H_

enum stream_status {
        STREAM_STATUS_OK,
        STREAM_STATUS_FINISHED,
        STREAM_STATUS_FAIL
};

/**
 * Initialize a new audio stream.
 * @param url URL to stream source
 * @param buf Buffer to write incoming data to
 * @param buf_size Size of provided buffer
 * @return Stream object; NULL if unsuccessful
 */
stream_t *stream_init(char *url, char *buf, unsigned int buf_size);

/**
 * Pull data from stream.
 */
int stream_pull_data(stream_t *st);

void stream_finish(stream_t *st);

#endif
