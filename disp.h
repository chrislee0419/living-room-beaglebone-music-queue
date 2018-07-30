#ifndef _DISP_H_
#define _DISP_H_

/**
 * Display module - Draws numbers on the Zen Cape display
 */

/**
 * Initializes module and thread
 * @return 0 if successful, otherwise error
 */
int disp_init(void);

/**
 * Cleans up the module and destroys the associated thread
 */
void disp_cleanup(void);

/**
 * Set the number to be displayed
 * @param number Number between 0 and 99 to be displayed
 */
void disp_setNumber(int number);

#endif
