#ifndef __BONGO__H__
#define __BONGO__H__

/* C std */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* HAL / device */
#include "py32f0xx_hal.h"
#include "py32f0xx_hal_flash.h"
#include "py32f0xx_hal_flash_ex.h"

/* Display */
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#include "bitmaps.h"
#include "bitmaps_angry.h"

/* ==== Button GPIO pins ==== */
#define SW_LEFT_GPIO_Port   GPIOA
#define SW_LEFT_Pin         GPIO_PIN_0
#define SW_RIGHT_GPIO_Port  GPIOA
#define SW_RIGHT_Pin        GPIO_PIN_1

/* ==== Tunables ==== */
#define IDLE_TIME               2000
#define TAP_DECAY_TIME          200
#define RESET_CONFIRM_TIMEOUT   10000

#define LEFT_PRESSED  (sw_state_left == 1 && sw_state_right == 0)
#define RIGHT_PRESSED (sw_state_left == 0 && sw_state_right == 1)
#define BOTH_PRESSED  (sw_state_left == 1 && sw_state_right == 1)
#define NONE_PRESSED  (sw_state_left == 0 && sw_state_right == 0)

/* Flash placement (last 128-byte page of 32KB PY32F030x6) */
#define FLASH_USER_START_ADDR   0x08007F80
#define FLASH_USER_END_ADDR     0x08007FFF

#ifndef FLASH_PAGE_SIZE
/* PY32F030x8 uses 2KB pages */
#define FLASH_PAGE_SIZE         0x800
#endif

#define FLASH_SAVE_INTERVAL     30000  /* ms */
#define SAVED_DISPLAY_TIME      2000   /* ms */
#define MODE_SWITCH_TIME        2500   /* ms */
#define INVERT_HOLD_TIME        2000   /* ms */

#define TAP_HISTORY_SIZE        20
#define TAP_SPEED_WINDOW        1000

#define ANGRY_MODE_ON_THRESHOLD   90   /* >= 9.0 taps/sec (x10) */
#define ANGRY_MODE_OFF_THRESHOLD  80   /* < 8.0 taps/sec (x10) */
#define ANGRY_MODE_DECAY_TIME     500

typedef enum states {
    IDLE = 0,
    SWITCH,
} state_e;

/* Persistent settings block (must be page-programmed) */
typedef struct {
    uint32_t total_taps;
    uint32_t left_taps;
    uint32_t right_taps;
    uint8_t  display_inverted;
    uint8_t  display_mode;
    uint8_t  reserved1;
    uint8_t  reserved2;
    uint32_t checksum;
} Settings_t;

/* ==== Globals provided in other modules ==== */
extern volatile int sw_state_left;
extern volatile int sw_state_right;

extern uint8_t     idle_cnt;
extern Settings_t  settings;

extern uint32_t    both_pressed_timer;
extern uint32_t    invert_timer;

extern uint32_t    last_save_time;
extern volatile uint8_t data_changed;

extern uint32_t    saved_indicator_timer;
extern uint8_t     show_saved_indicator;

/* current frame pointer for redraws */
extern const uint8_t *current_frame;

/* tap-speed state */
extern uint32_t    tap_timestamps[TAP_HISTORY_SIZE];
extern uint8_t     tap_history_index;
extern uint16_t    current_tap_speed_x10;

extern uint32_t    angry_mode_timer;

/* Image assets (bitmaps) provided elsewhere */
extern const uint8_t img_tap_left[];
extern const uint8_t img_tap_right[];
extern const uint8_t img_both_down_alt[];
extern const uint8_t img_both_down_alt_angry[];
extern const uint8_t img_right_down_alt[];
extern const uint8_t img_right_down_alt_angry[];
extern const uint8_t img_left_down_alt[];
extern const uint8_t img_left_down_alt_angry[];
extern const uint8_t img_both_up[];
extern const uint8_t img_both_up_angry[];

/* ==== API ==== */
uint32_t calculate_checksum(Settings_t *s);
uint8_t  use_angry_mode(void);
void     calculate_tap_speed(void);
void     record_tap_timestamp(void);
void     toggle_display_invert(void);

/* bitmap helpers (const-correct) */
void draw_animation(const uint8_t *frame);
void draw_animation_no_clear(const uint8_t *frame);
void draw_animation_erase(const uint8_t *frame);
void draw_animation_transparent(const uint8_t *frame);
void draw_idle_frame(uint8_t idx);
uint8_t idle_frame_count(void);

void readPins(void);

/* overlays / drawing */
void display_tap_count_overlay(void);
void display_saved_indicator(void);
void update_display_with_overlays(void);
void redraw_current_frame(void);

/* persistence */
void save_settings(void);
void load_settings(void);
void reset_all_settings(void);
void check_and_save(void);
void force_save(void);
void update_saved_indicator(void);

/* input/behavior */
uint8_t handle_display_mode_switch(void);
uint8_t handle_invert_toggle(void);
void    register_tap(uint8_t is_left);
void    handle_tap_decay(int32_t *tap_left_cntr, int32_t *tap_right_cntr);
void    handle_paw_animations(uint8_t *left_state, uint8_t *right_state,
                              int32_t *tap_left_cntr, int32_t *tap_right_cntr,
                              int32_t *idle_cntr);
uint8_t check_idle_transition(int32_t *idle_cntr, uint8_t *left_state, uint8_t *right_state);
void    handle_reset(void);
void    handle_credits(void);
void    handle_boot_overrides(void);
void    update_tap_speed(void);

#endif /* __BONGO__H__ */
