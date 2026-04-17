#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "main.h"

#define FLASH_USER_START_ADDR  0x0800F800
#define FLASH_SAVE_INTERVAL    30000
#define SAVED_DISPLAY_TIME     2000

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

extern Settings_t settings;
extern uint8_t    data_changed;
extern uint8_t    show_saved_indicator;
extern uint32_t   saved_indicator_timer;
extern uint32_t   last_save_time;

uint32_t calculate_checksum(Settings_t *s);
void save_settings(void);
void load_settings(void);
void reset_all_settings(void);
void check_and_save(void);
void force_save(void);
void update_saved_indicator(void);

#endif /* __SETTINGS_H */
