#include "animation.h"
#include "buttons.h"
#include "tap_tracker.h"
#include "settings.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "bitmap_extended.h"
#include "bitmap_extended_angry.h"
#include <stdio.h>
#include <string.h>

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
    uint8_t redraw = 0;

    if (*tap_left_cntr > 0 && HAL_GetTick() - *tap_left_cntr > TAP_DECAY_TIME) {
        *tap_left_cntr = 0;
        redraw = 1;
    }
    if (*tap_right_cntr > 0 && HAL_GetTick() - *tap_right_cntr > TAP_DECAY_TIME) {
        *tap_right_cntr = 0;
        redraw = 1;
    }

    // When an indicator expires, redraw the base frame to restore any cat pixels
    // that were underneath the overlay, then re-apply whatever is still active.
    if (redraw) {
        draw_animation(current_frame);
    }

    if (*tap_left_cntr > 0)  draw_animation_transparent(img_tap_left);
    if (*tap_right_cntr > 0) draw_animation_transparent(img_tap_right);
}

void handle_paw_animations(uint8_t *left_state, uint8_t *right_state,
                            int32_t *tap_left_cntr, int32_t *tap_right_cntr,
                            int32_t *idle_cntr) {
    *idle_cntr = 0;

    // Consume release flags: if a button was released during the last display
    // update (I2C is slow), reset its state so a re-press is treated as new.
    uint8_t rl = sw_released_left;  sw_released_left  = 0;
    uint8_t rr = sw_released_right; sw_released_right = 0;
    if (rl) *left_state  = 0;
    if (rr) *right_state = 0;

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

void draw_sleep_frame(void) {
    draw_animation(img_idle_sleep);
}

void draw_zzz_overlay(uint8_t frame) {
    // 4-phase cascade: z characters float diagonally upward, each 8px per phase
    switch (frame & 3) {
        case 0:
            ssd1306_SetCursor(108, 30); ssd1306_WriteString("z", Font_6x8, White);
            break;
        case 1:
            ssd1306_SetCursor(108, 22); ssd1306_WriteString("z", Font_6x8, White);
            ssd1306_SetCursor(100, 30); ssd1306_WriteString("z", Font_6x8, White);
            break;
        case 2:
            ssd1306_SetCursor(108, 14); ssd1306_WriteString("z", Font_6x8, White);
            ssd1306_SetCursor(100, 22); ssd1306_WriteString("z", Font_6x8, White);
            ssd1306_SetCursor(92,  30); ssd1306_WriteString("z", Font_6x8, White);
            break;
        case 3:
            ssd1306_SetCursor(100, 14); ssd1306_WriteString("z", Font_6x8, White);
            ssd1306_SetCursor(92,  22); ssd1306_WriteString("z", Font_6x8, White);
            break;
    }
}

void play_milestone_celebration(uint32_t milestone) {
    char buf[12];
    sprintf(buf, "%lu!", (unsigned long)milestone);

    uint8_t len   = (uint8_t)strlen(buf);
    uint8_t x_pos = (128 - len * 11) / 2;
    uint8_t y_pos = 22;

    for (int i = 0; i < 3; i++) {
        draw_animation(img_both_up);
        ssd1306_FillRectangle(x_pos - 2, y_pos - 2, x_pos + len * 11 + 2, y_pos + 20, Black);
        ssd1306_SetCursor(x_pos, y_pos);
        ssd1306_WriteString(buf, Font_11x18, White);
        ssd1306_UpdateScreen();
        HAL_Delay(250);

        ssd1306_InvertRectangle(x_pos - 2, y_pos - 2, x_pos + len * 11 + 2, y_pos + 20);
        ssd1306_UpdateScreen();
        HAL_Delay(100);
    }

    draw_animation(img_both_up);
    ssd1306_UpdateScreen();
    HAL_Delay(300);
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
