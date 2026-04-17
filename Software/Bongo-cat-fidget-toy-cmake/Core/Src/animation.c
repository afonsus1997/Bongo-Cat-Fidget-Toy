#include "animation.h"
#include "buttons.h"
#include "tap_tracker.h"
#include "settings.h"
#include "ssd1306.h"
#include "bitmap_extended.h"
#include "bitmap_extended_angry.h"

const unsigned char *current_frame = NULL;

void draw_animation(const unsigned char *frame) {
    ssd1306_Fill(Black);
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, White);
    current_frame = frame;
}

void draw_animation_no_clear(const unsigned char *frame) {
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, White);
    current_frame = frame;
}

void draw_animation_erase(const unsigned char *frame) {
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, Black);
}

void draw_animation_transparent(const unsigned char *frame) {
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, White);
}

void handle_tap_decay(int32_t *tap_left_cntr, int32_t *tap_right_cntr) {
    if (*tap_left_cntr > 0) {
        if (HAL_GetTick() - *tap_left_cntr > TAP_DECAY_TIME) {
            draw_animation_erase(img_tap_left);
            *tap_left_cntr = 0;
        } else {
            draw_animation_transparent(img_tap_left);
        }
    }
    if (*tap_right_cntr > 0) {
        if (HAL_GetTick() - *tap_right_cntr > TAP_DECAY_TIME) {
            draw_animation_erase(img_tap_right);
            *tap_right_cntr = 0;
        } else {
            draw_animation_transparent(img_tap_right);
        }
    }
}

void handle_paw_animations(uint8_t *left_state, uint8_t *right_state,
                            int32_t *tap_left_cntr, int32_t *tap_right_cntr,
                            int32_t *idle_cntr) {
    *idle_cntr = 0;

    uint8_t angry = use_angry_mode();

    if (BOTH_PRESSED && ((*left_state | *right_state) == 0 || (*left_state ^ *right_state) == 1)) {
        draw_animation(angry ? img_both_down_alt_angry : img_both_down_alt);
        if (!*right_state) {
            draw_animation_transparent(img_tap_right);
            *tap_right_cntr = HAL_GetTick();
            register_tap(0);
        }
        if (!*left_state) {
            draw_animation_transparent(img_tap_left);
            *tap_left_cntr = HAL_GetTick();
            register_tap(1);
        }
        *right_state = 1;
        *left_state  = 1;
    } else if (RIGHT_PRESSED) {
        if (*right_state == 0 || *left_state == 1) {
            draw_animation(angry ? img_right_down_alt_angry : img_right_down_alt);
            if (!*right_state) {
                draw_animation_transparent(img_tap_right);
                *tap_right_cntr = HAL_GetTick();
                register_tap(0);
            }
            *right_state = 1;
        }
        if (*left_state)
            *left_state = 0;
    } else if (LEFT_PRESSED) {
        if (*left_state == 0 || *right_state == 1) {
            draw_animation(angry ? img_left_down_alt_angry : img_left_down_alt);
            if (!*left_state) {
                draw_animation_transparent(img_tap_left);
                *tap_left_cntr = HAL_GetTick();
                register_tap(1);
            }
            *left_state = 1;
        }
        if (*right_state)
            *right_state = 0;
    }
}

void draw_idle_frame(uint8_t idx) {
    draw_animation(ani_idle[idx % ani_idle_LEN]);
}

uint8_t idle_frame_count(void) {
    return ani_idle_LEN;
}

uint8_t check_idle_transition(int32_t *idle_cntr, uint8_t *left_state, uint8_t *right_state) {
    if (NONE_PRESSED) {
        uint8_t angry = use_angry_mode();
        draw_animation(angry ? img_both_up_angry : img_both_up);

        if (*idle_cntr == 0) {
            *idle_cntr = HAL_GetTick();
        }
        if (HAL_GetTick() - *idle_cntr >= IDLE_TIME) {
            *idle_cntr = 0;
            force_save();
            return 1;
        }
        if (*left_state)
            *left_state = 0;
        if (*right_state)
            *right_state = 0;
    }
    return 0;
}
