#include "tap_tracker.h"
#include "settings.h"

static uint32_t tap_timestamps[TAP_HISTORY_SIZE] = {0};
static uint8_t  tap_history_index = 0;
static uint32_t angry_mode_timer  = 0;
uint16_t current_tap_speed_x10    = 0;
uint32_t pending_milestone        = 0;

static uint8_t is_milestone(uint32_t n) {
    if (n == 0)        return 0;
    if (n < 1000)      return (n % 100    == 0);  /* 100, 200, ..., 900   */
    if (n < 10000)     return (n % 1000   == 0);  /* 1k, 2k, ..., 9k      */
    if (n < 100000)    return (n % 10000  == 0);  /* 10k, 20k, ..., 90k   */
    if (n < 1000000)   return (n % 100000 == 0);  /* 100k, 200k, ..., 900k*/
    return                    (n % 1000000 == 0); /* 1M, 2M, ...          */
}

void tap_tracker_reset(void) {
    for (int i = 0; i < TAP_HISTORY_SIZE; i++) {
        tap_timestamps[i] = 0;
    }
    tap_history_index   = 0;
    current_tap_speed_x10 = 0;
    angry_mode_timer    = 0;
}

uint8_t use_angry_mode(void) {
    static uint8_t angry_active = 0;
    uint32_t now = HAL_GetTick();

    if (current_tap_speed_x10 >= ANGRY_MODE_ON_THRESHOLD) {
        angry_mode_timer = now;
        angry_active     = 1;
    } else if (angry_active && (current_tap_speed_x10 < ANGRY_MODE_OFF_THRESHOLD)) {
        if ((now - angry_mode_timer) >= ANGRY_MODE_DECAY_TIME) {
            angry_active = 0;
        }
    }

    return angry_active;
}

void calculate_tap_speed(void) {
    uint32_t current_time     = HAL_GetTick();
    int      valid_taps       = 0;
    uint32_t oldest_valid_time = current_time;

    for (int i = 0; i < TAP_HISTORY_SIZE; i++) {
        if (tap_timestamps[i] != 0 && (current_time - tap_timestamps[i]) <= TAP_SPEED_WINDOW) {
            valid_taps++;
            if (tap_timestamps[i] < oldest_valid_time) {
                oldest_valid_time = tap_timestamps[i];
            }
        }
    }

    if (valid_taps >= 2 && oldest_valid_time < current_time) {
        uint32_t time_span = current_time - oldest_valid_time;
        if (time_span > 0) {
            current_tap_speed_x10 = ((valid_taps - 1) * 10000) / time_span;
        } else {
            current_tap_speed_x10 = 0;
        }
    } else {
        current_tap_speed_x10 = 0;
    }
}

void record_tap_timestamp(void) {
    tap_timestamps[tap_history_index] = HAL_GetTick();
    tap_history_index = (tap_history_index + 1) % TAP_HISTORY_SIZE;
    calculate_tap_speed();
}

void update_tap_speed(void) {
    static uint32_t last_speed_update = 0;
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_speed_update >= 100) {
        calculate_tap_speed();
        last_speed_update = current_time;
    }
}

void register_tap(uint8_t is_left) {
    if (settings.total_taps < UINT32_MAX) settings.total_taps++;
    if (is_left) { if (settings.left_taps  < UINT32_MAX) settings.left_taps++;  }
    else         { if (settings.right_taps < UINT32_MAX) settings.right_taps++; }

    data_changed = 1;
    record_tap_timestamp();

    if (!pending_milestone && is_milestone(settings.total_taps)) {
        pending_milestone = settings.total_taps;
    }
}
