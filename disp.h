#ifndef _DISP_H_
#define _DISP_H_

/**
 * Initializes this module
 * @return 0 if successful, otherwise error
 */
int disp_init(void);

/**
 * Cleans up this module
 */
void disp_cleanup(void);

/**
 * Set the value to be displayed
 * @param val Value
 */
void disp_setValue(int val);

#endif
