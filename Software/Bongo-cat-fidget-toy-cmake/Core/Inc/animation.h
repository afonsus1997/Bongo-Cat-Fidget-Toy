#ifndef __ANIMATION_H
#define __ANIMATION_H

#include "main.h"

#define IDLE_TIME      2000
#define TAP_DECAY_TIME 200

extern const unsigned char *current_frame;

void    draw_animation(const unsigned char *frame);
void    draw_animation_no_clear(const unsigned char *frame);
void    draw_animation_erase(const unsigned char *frame);
void    draw_animation_transparent(const unsigned char *frame);
void    handle_tap_decay(int32_t *tap_left_cntr, int32_t *tap_right_cntr);
void    handle_paw_animations(uint8_t *left_state, uint8_t *right_state,
                               int32_t *tap_left_cntr, int32_t *tap_right_cntr,
                               int32_t *idle_cntr);
uint8_t check_idle_transition(int32_t *idle_cntr, uint8_t *left_state, uint8_t *right_state);
void    draw_idle_frame(uint8_t idx);
uint8_t idle_frame_count(void);

#endif /* __ANIMATION_H */
