#ifndef _DECODER_H_
#define _DECODER_H_

enum decoder_status {
        DECODER_STATUS_SUCCESS,
        DECODER_STATUS_NEED_MORE_DATA,
        DECODER_STATUS_FAIL
};

// NOTE: not sure if this is even necessary yet
/**
 * Initializes this module
 * @return 0 if successful, otherwise error
 */
int decoder_init(void);

/**
 * Cleans up this module
 */
void decoder_cleanup(void);

enum decoder_status decoder_init_stream(char *buf, unsigned int length);

enum decoder_status decoder_proc_stream(char *buf, unsigned int length);

#endif
