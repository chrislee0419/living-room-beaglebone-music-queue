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

#endif
