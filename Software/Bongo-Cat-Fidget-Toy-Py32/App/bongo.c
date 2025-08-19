#include "bongo.h"

/* ===== Internals ===== */
const uint8_t *current_frame = NULL;

/* ==== Checksum ==== */
uint32_t calculate_checksum(Settings_t *s) {
    uint32_t checksum = 0x12345678u;
    checksum ^= s->total_taps;
    checksum ^= s->left_taps;
    checksum ^= s->right_taps;
    checksum ^= ((uint32_t)s->display_inverted << 8);
    checksum ^= ((uint32_t)s->display_mode     << 16);
    return checksum;
}

/* Angry animation mode decision */
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

/* Calculate current tap speed (taps/sec * 10) */
void calculate_tap_speed(void) {
    uint32_t current_time = HAL_GetTick();
    int valid_taps = 0;
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
        uint32_t time_span = current_time - oldest_valid_time; // ms
        if (time_span > 0) {
            current_tap_speed_x10 = ((valid_taps - 1) * 10000u) / time_span;
        } else {
            current_tap_speed_x10 = 0;
        }
    } else {
        current_tap_speed_x10 = 0;
    }
}

/* Record a new tap for speed tracking */
void record_tap_timestamp(void) {
    tap_timestamps[tap_history_index] = HAL_GetTick();
    tap_history_index = (uint8_t)((tap_history_index + 1) % TAP_HISTORY_SIZE);
    calculate_tap_speed();
}

/* Toggle inversion + visual blink */
void toggle_display_invert(void) {
    settings.display_inverted = (uint8_t)!settings.display_inverted;
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

/* ===== Bitmap helpers (const-correct) ===== */
void draw_animation(const uint8_t *frame) {
    ssd1306_Fill(Black);
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, White);
    current_frame = frame;
}

void draw_animation_no_clear(const uint8_t *frame) {
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, White);
    current_frame = frame;
}

void draw_animation_erase(const uint8_t *frame) {
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, Black);
}

void draw_animation_transparent(const uint8_t *frame) {
    ssd1306_DrawBitmap(0, 0, frame, 128, 64, White);
}

void readPins(void) {
    /* Example if you wire them:
       sw_state_left  = HAL_GPIO_ReadPin(SW_LEFT_GPIO_Port, SW_LEFT_Pin);
       sw_state_right = HAL_GPIO_ReadPin(SW_RIGHT_GPIO_Port, SW_RIGHT_Pin);
    */
}

/* ===== Overlays ===== */
void display_tap_count_overlay(void) {
    char buffer[32];

    /* total taps top-left */
    sprintf(buffer, "%lu", (unsigned long)settings.total_taps);
    ssd1306_SetCursor(2, 1);
    ssd1306_WriteString(buffer, ComicSans_11x12, White);

    /* tap speed top-right */
    if (current_tap_speed_x10 > 1) {
        uint16_t whole   = (uint16_t)(current_tap_speed_x10 / 10u);
        uint16_t decimal = (uint16_t)(current_tap_speed_x10 % 10u);

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

/* ===== Persistence (Flash) ===== */
void save_settings(void) {
    /* Update checksum first */
    settings.checksum = calculate_checksum(&settings);

    /* Prepare one full page buffer (flash requires page programming) */
    uint32_t page_buf[FLASH_PAGE_SIZE / sizeof(uint32_t)];
    for (size_t i = 0; i < (sizeof(page_buf) / sizeof(page_buf[0])); ++i) {
        page_buf[i] = 0xFFFFFFFFu;
    }
    memcpy(page_buf, &settings, sizeof(settings));

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_USER_START_ADDR;
    EraseInitStruct.NbPages     = 1;

    (void)HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    /* Program a whole page */
    (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE,
                            FLASH_USER_START_ADDR,
                            (uint32_t *)page_buf);

    HAL_FLASH_Lock();

    /* Book-keeping */
    last_save_time = HAL_GetTick();
    data_changed = 0;

    show_saved_indicator = 1;
    saved_indicator_timer = HAL_GetTick();
}

void load_settings(void) {
    Settings_t loaded_settings;
    memset(&loaded_settings, 0xFF, sizeof(loaded_settings)); /* in case flash is blank */
    memcpy(&loaded_settings, (const void *)FLASH_USER_START_ADDR, sizeof(loaded_settings));

    if (loaded_settings.checksum == calculate_checksum(&loaded_settings) &&
        loaded_settings.checksum != 0xFFFFFFFFu) {
        settings = loaded_settings;
    } else {
        settings.total_taps       = 0;
        settings.left_taps        = 0;
        settings.right_taps       = 0;
        settings.display_inverted = 0;
        settings.display_mode     = 0;
        settings.reserved1        = 0;
        settings.reserved2        = 0;
        settings.checksum         = 0;
    }
}

void reset_all_settings(void) {
    if (HAL_GetTick() < 100) {
        HAL_Delay(100);
    }

    settings.total_taps       = 0;
    settings.left_taps        = 0;
    settings.right_taps       = 0;
    settings.display_inverted = 0;
    settings.display_mode     = 1;

    for (int i = 0; i < TAP_HISTORY_SIZE; i++) tap_timestamps[i] = 0;
    tap_history_index = 0;
    current_tap_speed_x10 = 0;
    angry_mode_timer = 0;

    save_settings();
}

void check_and_save(void) {
    if (data_changed && (HAL_GetTick() - last_save_time >= FLASH_SAVE_INTERVAL)) {
        save_settings();
    }
}

void force_save(void) {
    if (data_changed) {
        save_settings();
    }
}

void update_saved_indicator(void) {
    if (show_saved_indicator && (HAL_GetTick() - saved_indicator_timer >= SAVED_DISPLAY_TIME)) {
        show_saved_indicator = 0;
    }
}

/* ===== Buttons / modes ===== */
uint8_t handle_display_mode_switch(void) {
    if (BOTH_PRESSED) {
        if (both_pressed_timer == 0) {
            both_pressed_timer = HAL_GetTick();
        } else if (HAL_GetTick() - both_pressed_timer >= MODE_SWITCH_TIME) {
            settings.display_mode = (uint8_t)!settings.display_mode;
            both_pressed_timer = 0;
            data_changed = 1;

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

/* ===== Tap handling & animations ===== */
void register_tap(uint8_t is_left) {
    if (is_left) settings.left_taps++;
    else         settings.right_taps++;

    settings.total_taps++;
    data_changed = 1;

    record_tap_timestamp();
}

void handle_tap_decay(int32_t *tap_left_cntr, int32_t *tap_right_cntr) {
    if (*tap_left_cntr > 0) {
        if (HAL_GetTick() - (uint32_t)(*tap_left_cntr) > TAP_DECAY_TIME) {
            draw_animation_erase(img_tap_left);
            *tap_left_cntr = 0;
        } else {
            draw_animation_transparent(img_tap_left);
        }
    }
    if (*tap_right_cntr > 0) {
        if (HAL_GetTick() - (uint32_t)(*tap_right_cntr) > TAP_DECAY_TIME) {
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
            *tap_right_cntr = (int32_t)HAL_GetTick();
            register_tap(0);
        }
        if (!*left_state) {
            draw_animation_transparent(img_tap_left);
            *tap_left_cntr = (int32_t)HAL_GetTick();
            register_tap(1);
        }
        *right_state = 1;
        *left_state  = 1;
    } else if (RIGHT_PRESSED) {
        if (*right_state == 0 || *left_state == 1) {
            draw_animation(angry ? img_right_down_alt_angry : img_right_down_alt);
            if (!*right_state) {
                draw_animation_transparent(img_tap_right);
                *tap_right_cntr = (int32_t)HAL_GetTick();
                register_tap(0);
            }
            *right_state = 1;
        }
        if (*left_state) *left_state = 0;
    } else if (LEFT_PRESSED) {
        if (*left_state == 0 || *right_state == 1) {
            draw_animation(angry ? img_left_down_alt_angry : img_left_down_alt);
            if (!*left_state) {
                draw_animation_transparent(img_tap_left);
                *tap_left_cntr = (int32_t)HAL_GetTick();
                register_tap(1);
            }
            *left_state = 1;
        }
        if (*right_state) *right_state = 0;
    }
}

uint8_t check_idle_transition(int32_t *idle_cntr, uint8_t *left_state, uint8_t *right_state) {
    if (NONE_PRESSED) {
        uint8_t angry = use_angry_mode();
        draw_animation(angry ? img_both_up_angry : img_both_up);
        if (*idle_cntr == 0) {
            *idle_cntr = (int32_t)HAL_GetTick();
        }
        if (HAL_GetTick() - (uint32_t)(*idle_cntr) >= IDLE_TIME) {
            *idle_cntr = 0;
            force_save();
            return 1;
        }
        if (*left_state)  *left_state  = 0;
        if (*right_state) *right_state = 0;
    }
    return 0;
}

/* ===== Reset / Credits / Boot overrides ===== */
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
    uint8_t confirmed = 0;

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
        if (RIGHT_PRESSED || LEFT_PRESSED || BOTH_PRESSED) break;
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

/* ===== Periodic tap-speed update ===== */
void update_tap_speed(void) {
    static uint32_t last_speed_update = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_speed_update >= 100) {
        calculate_tap_speed();
        last_speed_update = now;
    }
}
