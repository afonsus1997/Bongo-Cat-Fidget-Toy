#ifndef __BONGO__H__
#define __BONGO__H__



#include "main.h"
#include "ssd1306.h" 

#define IDLE_TIME 2000
#define TAP_DECAY_TIME 200
#define RESET_CONFIRM_TIMEOUT 10000

#define LEFT_PRESSED (sw_state_left == 0 && sw_state_right == 1)
#define RIGHT_PRESSED (sw_state_left == 1 && sw_state_right == 0)
#define BOTH_PRESSED (sw_state_left == 0 && sw_state_right == 0)
#define NONE_PRESSED (sw_state_left == 1 && sw_state_right == 1)

// Correct addresses for 64KB STM32G030K8
// #define FLASH_SIZE              0x10000      // 64KB
// #define FLASH_PAGE_SIZE         0x800        // 2KB per page
// #define FLASH_PAGE_NB           32           // 64KB / 2KB = 32 pages (0-31)

// Use the last page (page 31) for storage
#define FLASH_USER_START_ADDR   0x0800F800   // Start of last page (page 31)
#define FLASH_USER_END_ADDR     0x0800FFFF   // End of flash

// Flash save timing
#define FLASH_SAVE_INTERVAL 30000  // Save every 30 seconds (30000ms)
#define SAVED_DISPLAY_TIME 2000     // Show "saved!" for 2 seconds

// Timer for display mode
#define MODE_SWITCH_TIME 2500  // 2.5 seconds to switch display mode

// Timer for invert toggle
#define INVERT_HOLD_TIME 2000  // 2 seconds to toggle invert

// Tap speed tracking
#define TAP_HISTORY_SIZE 20    // Number of taps to track for speed calculation
#define TAP_SPEED_WINDOW 1000  // Time window in ms for speed calculation

// Angry mode settings
#define ANGRY_MODE_ON_THRESHOLD  90   // Turn on at ≥ 9 taps/sec
#define ANGRY_MODE_OFF_THRESHOLD 80   // Turn off only below 7 taps/sec
#define ANGRY_MODE_DECAY_TIME 500  // Stay angry for 1 second after speed drops

typedef enum states {
    IDLE,
    SWITCH,
} state_e;

// Settings structure for flash storage
typedef struct {
    uint32_t total_taps;
    uint32_t left_taps;
    uint32_t right_taps;
    uint8_t display_inverted;
    uint8_t display_mode;
    uint8_t reserved1;  // Padding
    uint8_t reserved2;  // Padding
    uint32_t checksum;  // Simple validation
} Settings_t;



uint32_t calculate_checksum(Settings_t *s);
uint8_t use_angry_mode(void);
void calculate_tap_speed(void);
void record_tap_timestamp(void);
void toggle_display_invert(void);
void draw_animation(char* frame);
void draw_animation_no_clear(char* frame);
void draw_animation_erase(char* frame);
void draw_animation_transparent(char* frame);
void readPins();



#endif  //!__BONGO__H__