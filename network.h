#ifndef _NETWORK_H_
#define _NETWORK_H_

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
 */
void network_sendPlayCmd(void);

/**
 * Send the pause song command to the master device
 */
void network_sendPauseCmd(void);

/**
 * Send the skip song command to the master device
 */
void network_sendSkipCmd(void);

/**
 * Send audio data to slave devices
 */
void network_sendAudio(char *buf, int len);

#endif
