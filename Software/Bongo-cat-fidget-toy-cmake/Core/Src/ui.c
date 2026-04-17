#include "ui.h"
#include "settings.h"
#include "tap_tracker.h"
#include "animation.h"
#include "buttons.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

static uint32_t both_pressed_timer = 0;
static uint32_t invert_timer       = 0;

void display_tap_count_overlay(void) {
    char buffer[32];

    sprintf(buffer, "%lu", settings.total_taps);
    ssd1306_SetCursor(2, 1);
    ssd1306_WriteString(buffer, ComicSans_11x12, White);

    if (current_tap_speed_x10 > 1) {
        uint16_t whole   = current_tap_speed_x10 / 10;
        uint16_t decimal = current_tap_speed_x10 % 10;

        if (whole >= 10) {
            sprintf(buffer, "%u/s", whole);
        } else {
            sprintf(buffer, "%u.%u/s", whole, decimal);
        }

        ssd1306_SetCursor(95, 1);
        ssd1306_WriteString(buffer, ComicSans_11x12, White);
    }
}

void display_saved_indicator(void) {
    ssd1306_SetCursor(80, 52);
    ssd1306_WriteString("saved!", ComicSans_11x12, White);
}

void update_display_with_overlays(void) {
    if (settings.display_mode) {
        display_tap_count_overlay();
    }
    if (show_saved_indicator) {
        display_saved_indicator();
    }
    ssd1306_UpdateScreen();
}

void redraw_current_frame(void) {
    if (current_frame != NULL) {
        draw_animation(current_frame);
        update_display_with_overlays();
    }
}

void toggle_display_invert(void) {
    settings.display_inverted = !settings.display_inverted;
    ssd1306_InvertDisplay(settings.display_inverted);
    data_changed = 1;

    for (int i = 0; i < 2; i++) {
        ssd1306_Fill(White);
        ssd1306_UpdateScreen();
        HAL_Delay(50);
        ssd1306_Fill(Black);
        ssd1306_UpdateScreen();
        HAL_Delay(50);
    }
}

uint8_t handle_display_mode_switch(void) {
    if (BOTH_PRESSED) {
        if (both_pressed_timer == 0) {
            both_pressed_timer = HAL_GetTick();
        } else if (HAL_GetTick() - both_pressed_timer >= MODE_SWITCH_TIME) {
            settings.display_mode = !settings.display_mode;
            both_pressed_timer    = 0;
            data_changed          = 1;
            redraw_current_frame();
            force_save();
            return 1;
        }
    } else {
        both_pressed_timer = 0;
    }
    return 0;
}

uint8_t handle_invert_toggle(void) {
    if (LEFT_PRESSED && !RIGHT_PRESSED && both_pressed_timer == 0) {
        if (invert_timer == 0) {
            invert_timer = HAL_GetTick();
        } else if (HAL_GetTick() - invert_timer >= INVERT_HOLD_TIME) {
            toggle_display_invert();
            invert_timer = 0;
            while (LEFT_PRESSED) {
                HAL_Delay(10);
                readPins();
            }
            return 1;
        }
    } else {
        invert_timer = 0;
    }
    return 0;
}

void handle_reset(void) {
    ssd1306_SetCursor(27, 10);
    ssd1306_WriteString("RESET ALL?", ComicSans_11x12, White);
    ssd1306_SetCursor(27, 26);
    ssd1306_WriteString("Press again", ComicSans_11x12, White);
    ssd1306_SetCursor(27, 42);
    ssd1306_WriteString("to confirm", ComicSans_11x12, White);
    ssd1306_UpdateScreen();

    while (RIGHT_PRESSED) {
        HAL_Delay(10);
        readPins();
    }

    uint32_t confirm_start = HAL_GetTick();
    uint8_t  confirmed     = 0;

    while (HAL_GetTick() - confirm_start < RESET_CONFIRM_TIMEOUT) {
        readPins();
        if (RIGHT_PRESSED) {
            confirmed = 1;
            break;
        }
        HAL_Delay(100);
    }

    if (confirmed) {
        reset_all_settings();
        ssd1306_InvertDisplay(0);

        ssd1306_Fill(Black);
        ssd1306_SetCursor(35, 20);
        ssd1306_WriteString("RESET OK!", ComicSans_11x12, White);
        ssd1306_UpdateScreen();
        HAL_Delay(1000);

        while (RIGHT_PRESSED) {
            HAL_Delay(10);
            readPins();
        }
    } else {
        ssd1306_Fill(Black);
        ssd1306_SetCursor(40, 25);
        ssd1306_WriteString("Cancelled", ComicSans_11x12, White);
        ssd1306_UpdateScreen();
        HAL_Delay(1000);
    }

    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
}

void handle_credits(void) {
    ssd1306_SetCursor(5, 10);
    ssd1306_WriteString("Bongo Cat Fidget Toy", ComicSans_11x12, White);
    ssd1306_SetCursor(13, 21);
    ssd1306_WriteString("by Afonso Muralha", ComicSans_11x12, White);
    ssd1306_SetCursor(13, 40);
    ssd1306_WriteString("afonsomuralha.com", ComicSans_11x12, White);
    ssd1306_UpdateScreen();

    while (BOTH_PRESSED || LEFT_PRESSED || RIGHT_PRESSED) {
        HAL_Delay(10);
        readPins();
    }

    while (1) {
        readPins();
        if (RIGHT_PRESSED || LEFT_PRESSED || BOTH_PRESSED) {
            break;
        }
        HAL_Delay(100);
    }

    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
}

void handle_boot_overrides(void) {
    readPins();

    if (BOTH_PRESSED) {
        handle_credits();
    } else {
        if (LEFT_PRESSED) {
            toggle_display_invert();
            force_save();
            while (LEFT_PRESSED) {
                HAL_Delay(10);
                readPins();
            }
        }
        if (RIGHT_PRESSED) {
            handle_reset();
        }
    }
}
