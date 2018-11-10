#ifndef _STREAM_H_
#define _STREAM_H_

#include <curl/curl.h>

enum stream_status {
        STREAM_STATUS_OK,
        STREAM_STATUS_OK_NODATA,
        STREAM_STATUS_COMPLETE,
        STREAM_STATUS_FAIL
};

typedef struct stream stream_t;


/**
 * Initialize a new audio stream.
 * @param url URL to stream source
 * @return Stream object; NULL if unsuccessful
 */
stream_t *stream_init(const char *url);

/**
 * Pull data from stream and store in the stream object.
 * @param st Stream object
 * @param buf Buffer to copy contents to
 * @param bytes Number of bytes to copy; after return, stores the number of bytes copied
 * @return Status of stream
 */
enum stream_status stream_pull_data(stream_t *st, char *buf, unsigned int *bytes);

/**
 * Destroy a stream object.
 * @param st Stream object
 */
void stream_free(stream_t *st);

#endif
