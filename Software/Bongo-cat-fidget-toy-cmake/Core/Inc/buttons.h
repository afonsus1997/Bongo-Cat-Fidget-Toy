#ifndef __BUTTONS_H
#define __BUTTONS_H

#include "main.h"

extern int sw_state_left;
extern int sw_state_right;

#define LEFT_PRESSED  (sw_state_left == 0 && sw_state_right == 1)
#define RIGHT_PRESSED (sw_state_left == 1 && sw_state_right == 0)
#define BOTH_PRESSED  (sw_state_left == 0 && sw_state_right == 0)
#define NONE_PRESSED  (sw_state_left == 1 && sw_state_right == 1)

void readPins(void);

#endif /* __BUTTONS_H */
