#ifndef __BONGO__H__
#define __BONGO__H__

#include "main.h"
#include "ssd1306.h" 

#define IDLE_TIME 2000
#define TAP_DECAY_TIME 200
#define RESET_CONFIRM_TIMEOUT 10000

#define LEFT_PRESSED  (sw_state_left == 0 && sw_state_right == 1)
#define RIGHT_PRESSED (sw_state_left == 1 && sw_state_right == 0)
#define BOTH_PRESSED  (sw_state_left == 0 && sw_state_right == 0)
#define NONE_PRESSED  (sw_state_left == 1 && sw_state_right == 1)

#define FLASH_USER_START_ADDR   0x0800F800
#define FLASH_USER_END_ADDR     0x0800FFFF

#define FLASH_SAVE_INTERVAL 30000
#define SAVED_DISPLAY_TIME  2000
#define MODE_SWITCH_TIME    2500
#define INVERT_HOLD_TIME    2000

#define TAP_HISTORY_SIZE   20
#define TAP_SPEED_WINDOW   1000

#define ANGRY_MODE_ON_THRESHOLD   90
#define ANGRY_MODE_OFF_THRESHOLD  80
#define ANGRY_MODE_DECAY_TIME     500

typedef enum states {
    IDLE,
    SWITCH,
} state_e;

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

/* ==== GLOBALS: declarations only ==== */
extern volatile int sw_state_left;
extern volatile int sw_state_right;

extern uint8_t   idle_cnt;
extern Settings_t settings;

extern uint32_t  both_pressed_timer;
extern uint32_t  invert_timer;

extern uint32_t  last_save_time;
extern volatile uint8_t data_changed;

extern uint32_t  saved_indicator_timer;
extern uint8_t   show_saved_indicator;

extern const char* current_frame;

extern uint32_t  tap_timestamps[TAP_HISTORY_SIZE];
extern uint8_t   tap_history_index;
extern uint16_t  current_tap_speed_x10;

extern uint32_t  angry_mode_timer;

/* ==== functions ==== */
uint32_t calculate_checksum(Settings_t *s);
uint8_t  use_angry_mode(void);
void     calculate_tap_speed(void);
void     record_tap_timestamp(void);
void     toggle_display_invert(void);
void     draw_animation(char* frame);
void     draw_animation_no_clear(char* frame);
void     draw_animation_erase(char* frame);
void     draw_animation_transparent(char* frame);
void     readPins(void);

#endif  // __BONGO__H__
