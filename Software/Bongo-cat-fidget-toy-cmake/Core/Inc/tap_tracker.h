#ifndef __TAP_TRACKER_H
#define __TAP_TRACKER_H

#include "main.h"

#define TAP_HISTORY_SIZE         20
#define TAP_SPEED_WINDOW         1000
#define TAP_SPEED_MIN_INTERVAL   5
#define ANGRY_MODE_ON_THRESHOLD  100
#define ANGRY_MODE_OFF_THRESHOLD 80
#define ANGRY_MODE_DECAY_TIME    500

extern uint16_t current_tap_speed_x10;
extern uint32_t pending_milestone;

void    tap_tracker_reset(void);
void    record_tap_timestamp(void);
void    calculate_tap_speed(void);
void    update_tap_speed(void);
uint8_t use_angry_mode(void);
void    register_tap(uint8_t is_left);

#endif /* __TAP_TRACKER_H */
