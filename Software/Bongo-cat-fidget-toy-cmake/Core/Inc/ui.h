#ifndef __UI_H
#define __UI_H

#include "main.h"

#define RESET_CONFIRM_TIMEOUT 10000
#define MODE_SWITCH_TIME      2500
#define INVERT_HOLD_TIME      2000

void    display_tap_count_overlay(void);
void    display_saved_indicator(void);
void    update_display_with_overlays(void);
void    redraw_current_frame(void);
void    toggle_display_invert(void);
uint8_t handle_display_mode_switch(void);
uint8_t handle_invert_toggle(void);
void    handle_reset(void);
void    handle_credits(void);
void    handle_boot_overrides(void);

#endif /* __UI_H */
