

#include "bongo.h"
#include "py32f0xx_ll_utils.h"

// Calculate simple checksum
uint32_t calculate_checksum(Settings_t *s) {
    uint32_t checksum = 0x12345678;  // Magic number
    checksum ^= s->total_taps;
    checksum ^= s->left_taps;
    checksum ^= s->right_taps;
    checksum ^= ((uint32_t)s->display_inverted << 8);
    checksum ^= ((uint32_t)s->display_mode << 16);
    return checksum;
}

uint8_t use_angry_mode(void) {
    static uint8_t angry_active = 0;
    uint32_t now = HAL_GetTick();

    if (current_tap_speed_x10 >= ANGRY_MODE_ON_THRESHOLD) {
        angry_mode_timer = now;
        angry_active = 1;
    } else if (angry_active && (current_tap_speed_x10 < ANGRY_MODE_OFF_THRESHOLD)) {
        if ((now - angry_mode_timer) >= ANGRY_MODE_DECAY_TIME) {
            angry_active = 0;
        }
    }

    return angry_active;
}

void calculate_tap_speed(void) {
    uint32_t current_time = HAL_GetTick();
    int valid_taps = 0;
    uint32_t oldest_valid_time = current_time;

    // Count taps within the time window
    for (int i = 0; i < TAP_HISTORY_SIZE; i++) {
        if (tap_timestamps[i] != 0 && (current_time - tap_timestamps[i]) <= TAP_SPEED_WINDOW) {
            valid_taps++;
            if (tap_timestamps[i] < oldest_valid_time) {
                oldest_valid_time = tap_timestamps[i];
            }
        }
    }

    // Calculate speed
    if (valid_taps >= 2 && oldest_valid_time < current_time) {
        uint32_t time_span = current_time - oldest_valid_time; // in milliseconds
        if (time_span > 0) {
            // Calculate taps per second * 10 (to get one decimal place)
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

void toggle_display_invert(void) {
    settings.display_inverted = !settings.display_inverted;
    ssd1306_InvertDisplay(settings.display_inverted);

    // Mark data as changed
    data_changed = 1;

    // Visual feedback
    for(int i = 0; i < 2; i++) {
        ssd1306_Fill(White);
        ssd1306_UpdateScreen();
        LL_mDelay(50);
        ssd1306_Fill(Black);
        ssd1306_UpdateScreen();
        LL_mDelay(50);
    }
}

void draw_animation(char* frame){
    ssd1306_Fill(Black);
    ssd1306_DrawBitmap(0,0,frame,128,64,White);
    current_frame = frame;  // Store current frame
}

void draw_animation_no_clear(char* frame){
    // Draw without clearing - useful for transitions
    ssd1306_DrawBitmap(0,0,frame,128,64,White);
    current_frame = frame;
}

void draw_animation_erase(char* frame){
    ssd1306_DrawBitmap(0,0,frame,128,64,Black);
}

void draw_animation_transparent(char* frame){
    ssd1306_DrawBitmap(0,0,frame,128,64,White);
}

void readPins(){
//TODO
  // sw_state_left = HAL_GPIO_ReadPin(SW_LEFT_GPIO_Port, SW_LEFT_Pin);
    // sw_state_right = HAL_GPIO_ReadPin(SW_RIGHT_GPIO_Port, SW_RIGHT_Pin);
}
