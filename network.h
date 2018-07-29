#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <netinet/in.h>

#define NETWORK_MAX_BUFFER_SIZE         1500

/**
 * Initializes this module
 * @return 0 if successful, otherwise error
 */
int network_init(void);

/**
 * Cleans up this module
 */
void network_cleanup(void);

/**
 * Send the play song command to the master device
 * @param addr Address of destination
 */
void network_sendPlayCmd(struct sockaddr_in addr);

/**
 * Send the pause song command to the master device
 * @param addr Address of destination
 */
void network_sendPauseCmd(struct sockaddr_in addr);

/**
 * Send the skip song command to the master device
 * @param addr Address of destination
 */
void network_sendSkipCmd(struct sockaddr_in addr);

/**
 * Multicast audio data.
 * @param buf Buffer of data to send
 * @param len Size of buffer
 */
void network_sendAudio(char *buf, unsigned int len);

#endif
