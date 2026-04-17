#include "settings.h"
#include "tap_tracker.h"

Settings_t settings         = {0};
uint8_t    data_changed      = 0;
uint8_t    show_saved_indicator = 0;
uint32_t   saved_indicator_timer = 0;
uint32_t   last_save_time    = 0;

uint32_t calculate_checksum(Settings_t *s) {
    uint32_t checksum = 0x12345678;
    checksum ^= s->total_taps;
    checksum ^= s->left_taps;
    checksum ^= s->right_taps;
    checksum ^= ((uint32_t)s->display_inverted << 8);
    checksum ^= ((uint32_t)s->display_mode << 16);
    return checksum;
}

void save_settings(void) {
    settings.checksum = calculate_checksum(&settings);

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Page      = 31;
    EraseInitStruct.NbPages   = 1;
    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    uint64_t *data    = (uint64_t *)&settings;
    uint32_t  address = FLASH_USER_START_ADDR;
    int doublewords   = (sizeof(Settings_t) + 7) / 8;

    for (int i = 0; i < doublewords; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data[i]);
        address += 8;
    }

    HAL_FLASH_Lock();

    last_save_time       = HAL_GetTick();
    data_changed         = 0;
    show_saved_indicator = 1;
    saved_indicator_timer = HAL_GetTick();
}

void load_settings(void) {
    Settings_t loaded_settings;

    uint64_t *dest       = (uint64_t *)&loaded_settings;
    uint64_t *src        = (uint64_t *)FLASH_USER_START_ADDR;
    int       doublewords = (sizeof(Settings_t) + 7) / 8;

    for (int i = 0; i < doublewords; i++) {
        dest[i] = src[i];
    }

    if (loaded_settings.checksum == calculate_checksum(&loaded_settings) &&
        loaded_settings.checksum != 0xFFFFFFFF) {
        settings = loaded_settings;
    } else {
        settings.total_taps      = 0;
        settings.left_taps       = 0;
        settings.right_taps      = 0;
        settings.display_inverted = 0;
        settings.display_mode    = 0;
        settings.checksum        = 0;
    }
}

void reset_all_settings(void) {
    if (HAL_GetTick() < 100) {
        HAL_Delay(100);
    }

    settings.total_taps      = 0;
    settings.left_taps       = 0;
    settings.right_taps      = 0;
    settings.display_inverted = 0;
    settings.display_mode    = 1;

    tap_tracker_reset();
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
