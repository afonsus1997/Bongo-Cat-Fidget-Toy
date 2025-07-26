/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "bitmap_extended.h"
#include "bitmap_extended_angry.h"
#include <stdio.h>  // For sprintf

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// State machine states
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
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IDLE_TIME 2000
#define TAP_DECAY_TIME 200
#define RESET_CONFIRM_TIMEOUT 10000

#define LEFT_PRESSED (sw_state_left == 0 && sw_state_right == 1)
#define RIGHT_PRESSED (sw_state_left == 1 && sw_state_right == 0)
#define BOTH_PRESSED (sw_state_left == 0 && sw_state_right == 0)
#define NONE_PRESSED (sw_state_left == 1 && sw_state_right == 1)

// Correct addresses for 64KB STM32G030K8
#define FLASH_SIZE              0x10000      // 64KB
#define FLASH_PAGE_SIZE         0x800        // 2KB per page
#define FLASH_PAGE_NB           32           // 64KB / 2KB = 32 pages (0-31)

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
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim14;

/* USER CODE BEGIN PV */
// Switch states
int sw_state_left;
int sw_state_right;

// Animation counter
uint8_t idle_cnt;

// Settings structure
Settings_t settings = {0};

// Timer for display mode
uint32_t both_pressed_timer = 0;

// Timer for invert toggle
uint32_t invert_timer = 0;

// Flash save management
uint32_t last_save_time = 0;
uint8_t data_changed = 0;  // Flag to track if data needs saving

// Saved indicator
uint32_t saved_indicator_timer = 0;
uint8_t show_saved_indicator = 0;

// Current animation frame storage
char* current_frame = NULL;

// Tap speed tracking
uint32_t tap_timestamps[TAP_HISTORY_SIZE] = {0};
uint8_t tap_history_index = 0;
uint16_t current_tap_speed_x10 = 0;  // Speed * 10 to avoid float

// Angry mode decay
uint32_t angry_mode_timer = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM14_Init(void);
static void MX_NVIC_Init(void);


/* USER CODE BEGIN PFP */

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

// Helper function to check if we should use angry animations
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



// Calculate current tap speed (taps per second * 10)
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

// Record a new tap for speed tracking
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
        HAL_Delay(50);
        ssd1306_Fill(Black);
        ssd1306_UpdateScreen();
        HAL_Delay(50);
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
    sw_state_left = HAL_GPIO_ReadPin(SW_LEFT_GPIO_Port, SW_LEFT_Pin);
    sw_state_right = HAL_GPIO_ReadPin(SW_RIGHT_GPIO_Port, SW_RIGHT_Pin);
}

// Display tap count as overlay
void display_tap_count_overlay(void) {
    char buffer[32];

    // Display total tap count in top left
    sprintf(buffer, "%lu", settings.total_taps);
    ssd1306_SetCursor(2, 1);
    ssd1306_WriteString(buffer, ComicSans_11x12, White);

    // Display tap speed in top right
    if (current_tap_speed_x10 > 1) {  // Only show if actively tapping
        // Format speed with one decimal place
        uint16_t whole = current_tap_speed_x10 / 10;
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

// Display saved indicator
void display_saved_indicator(void) {
    // Display "saved!" in bottom right corner
    ssd1306_SetCursor(80, 52);
    ssd1306_WriteString("saved!", ComicSans_11x12, White);
}

// Update display with overlays
void update_display_with_overlays(void) {
    // Draw overlay on top if enabled
    if (settings.display_mode) {
        display_tap_count_overlay();
    }

    // Show saved indicator if active
    if (show_saved_indicator) {
        display_saved_indicator();
    }

    ssd1306_UpdateScreen();
}

// Redraw current frame with overlays
void redraw_current_frame(void) {
    if (current_frame != NULL) {
        draw_animation(current_frame);
        update_display_with_overlays();
    }
}

// Save all settings to flash
void save_settings(void) {
    // Update checksum
    settings.checksum = calculate_checksum(&settings);

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Page = 31;  // Last page for 64KB device
    EraseInitStruct.NbPages = 1;

    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    // Write entire structure using doubleword programming
    uint64_t *data = (uint64_t*)&settings;
    uint32_t address = FLASH_USER_START_ADDR;

    // Calculate number of doublewords to write
    int doublewords = (sizeof(Settings_t) + 7) / 8;  // Round up to nearest 8 bytes

    for(int i = 0; i < doublewords; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data[i]);
        address += 8;
    }

    HAL_FLASH_Lock();

    // Update last save time
    last_save_time = HAL_GetTick();
    data_changed = 0;  // Clear the changed flag

    // Trigger saved indicator
    show_saved_indicator = 1;
    saved_indicator_timer = HAL_GetTick();
}

// Load all settings from flash
void load_settings(void) {
    Settings_t loaded_settings;

    // Read entire structure from flash
    uint64_t *dest = (uint64_t*)&loaded_settings;
    uint64_t *src = (uint64_t*)FLASH_USER_START_ADDR;

    int doublewords = (sizeof(Settings_t) + 7) / 8;

    for(int i = 0; i < doublewords; i++) {
        dest[i] = src[i];
    }

    // Verify checksum
    if(loaded_settings.checksum == calculate_checksum(&loaded_settings) &&
       loaded_settings.checksum != 0xFFFFFFFF) {
        // Valid data, copy to working structure
        settings = loaded_settings;
    } else {
        // Invalid or no data, use defaults
        settings.total_taps = 0;
        settings.left_taps = 0;
        settings.right_taps = 0;
        settings.display_inverted = 0;
        settings.display_mode = 0;
        settings.checksum = 0;
    }
}

// Reset all counters and settings
void reset_all_settings(void) {
    // Don't do flash operations too early
    if (HAL_GetTick() < 100) {
        HAL_Delay(100);  // Ensure system is stable
    }

    // Set values to defaults
    settings.total_taps = 0;
    settings.left_taps = 0;
    settings.right_taps = 0;
    settings.display_inverted = 0;
    settings.display_mode = 1;

    // Clear tap history
    for (int i = 0; i < TAP_HISTORY_SIZE; i++) {
        tap_timestamps[i] = 0;
    }
    tap_history_index = 0;
    current_tap_speed_x10 = 0;
    angry_mode_timer = 0;  // Clear angry mode timer

    // Save the reset settings
    save_settings();
}

// Check if it's time to save to flash
void check_and_save(void) {
    if (data_changed && (HAL_GetTick() - last_save_time >= FLASH_SAVE_INTERVAL)) {
        save_settings();
    }
}

// Force save (for important events)
void force_save(void) {
    if (data_changed) {
        save_settings();
    }
}

// Update saved indicator visibility
void update_saved_indicator(void) {
    if (show_saved_indicator && (HAL_GetTick() - saved_indicator_timer >= SAVED_DISPLAY_TIME)) {
        show_saved_indicator = 0;
    }
}

// Handle display mode switching (both buttons held)
uint8_t handle_display_mode_switch(void) {
    if (BOTH_PRESSED) {
        if (both_pressed_timer == 0) {
            both_pressed_timer = HAL_GetTick();
        } else if (HAL_GetTick() - both_pressed_timer >= MODE_SWITCH_TIME) {
            settings.display_mode = !settings.display_mode;
            both_pressed_timer = 0;
            data_changed = 1;  // Mark data as changed

            // Immediately update display to show/hide overlay
            redraw_current_frame();

            // Force save when switching modes
            force_save();

            return 1; // Mode switched
        }
    } else {
        both_pressed_timer = 0;
    }
    return 0; // No mode switch
}

// Handle invert toggle (left button held)
uint8_t handle_invert_toggle(void) {
    if (LEFT_PRESSED && !RIGHT_PRESSED && both_pressed_timer == 0) {
        if (invert_timer == 0) {
            invert_timer = HAL_GetTick();
        } else if (HAL_GetTick() - invert_timer >= INVERT_HOLD_TIME) {
            toggle_display_invert();
            invert_timer = 0;

            // Wait for button release
            while(LEFT_PRESSED) {
                HAL_Delay(10);
                readPins();
            }
            return 1; // Inverted
        }
    } else {
        invert_timer = 0;
    }
    return 0; // No invert
}

// Register a tap and increment counters
void register_tap(uint8_t is_left) {
    if (is_left) {
        settings.left_taps++;
    } else {
        settings.right_taps++;
    }
    settings.total_taps++;
    data_changed = 1;

    // Record timestamp for speed calculation
    record_tap_timestamp();
}

// Handle tap animations and decay
void handle_tap_decay(int32_t *tap_left_cntr, int32_t *tap_right_cntr) {

    if(*tap_left_cntr > 0){
        if(HAL_GetTick() - *tap_left_cntr > TAP_DECAY_TIME) {
            draw_animation_erase(&img_tap_left);
            *tap_left_cntr = 0;
        }
        else{
            draw_animation_transparent(&img_tap_left);
        }
    }
    if(*tap_right_cntr > 0){
        if(HAL_GetTick() - *tap_right_cntr > TAP_DECAY_TIME) {
            draw_animation_erase(&img_tap_right);
            *tap_right_cntr = 0;
        }
        else{
            draw_animation_transparent(&img_tap_right);
        }
    }
}

// Handle paw animations based on button states
void handle_paw_animations(uint8_t *left_state, uint8_t *right_state,
                          int32_t *tap_left_cntr, int32_t *tap_right_cntr,
                          int32_t *idle_cntr) {
    *idle_cntr = 0;

    uint8_t angry = use_angry_mode();
    if((BOTH_PRESSED) && ((*left_state | *right_state) == 0 || (*left_state ^ *right_state) == 1)){
        draw_animation(angry ? &img_both_down_alt_angry : &img_both_down_alt);
        if(!*right_state){
            draw_animation_transparent(&img_tap_right);
            *tap_right_cntr = HAL_GetTick();
            register_tap(0); // Right tap
        }
        if(!*left_state){
            draw_animation_transparent(&img_tap_left);
            *tap_left_cntr = HAL_GetTick();
            register_tap(1); // Left tap
        }
        *right_state = 1;
        *left_state = 1;
    }
    else if(RIGHT_PRESSED){
        if(*right_state == 0 || *left_state == 1){
            draw_animation(angry ? &img_right_down_alt_angry : &img_right_down_alt);
            if(!*right_state){
                draw_animation_transparent(&img_tap_right);
                *tap_right_cntr = HAL_GetTick();
                register_tap(0); // Right tap
            }
            *right_state = 1;
        }
        if(*left_state)
            *left_state = 0;
    }
    else if(LEFT_PRESSED){
        if(*left_state == 0 || *right_state == 1){
            draw_animation(angry ? &img_left_down_alt_angry : &img_left_down_alt);
            if(!*left_state){
                draw_animation_transparent(&img_tap_left);
                *tap_left_cntr = HAL_GetTick();
                register_tap(1); // Left tap
            }
            *left_state = 1;
        }
        if(*right_state)
            *right_state = 0;
    }
}

// Check if should return to idle state
uint8_t check_idle_transition(int32_t *idle_cntr, uint8_t *left_state, uint8_t *right_state) {
    if(NONE_PRESSED){
        uint8_t angry = use_angry_mode();
        draw_animation(angry ? &img_both_up_angry : &img_both_up);
        if(*idle_cntr == 0){
            *idle_cntr = HAL_GetTick();
        }
        if(HAL_GetTick() - *idle_cntr >= IDLE_TIME){
            *idle_cntr = 0;
            // Force save before going to idle
            force_save();
            return 1; // Should transition to idle
        }
        if(*left_state)
            *left_state = 0;
        if(*right_state)
            *right_state = 0;
    }
    return 0; // Stay in current state
}

void handle_reset(){
    // Show confirmation prompt
    ssd1306_SetCursor(27, 10);  // x = 9
    ssd1306_WriteString("RESET ALL?", ComicSans_11x12, White);
    ssd1306_SetCursor(27, 26);  // x = 3
    ssd1306_WriteString("Press again", ComicSans_11x12, White);
    ssd1306_SetCursor(27, 42);  // x = 9
    ssd1306_WriteString("to confirm", ComicSans_11x12, White);
    ssd1306_UpdateScreen();
    // Wait for button release
    while(RIGHT_PRESSED) {
        HAL_Delay(10);
        readPins();
    }

    // Wait for confirmation press
    uint32_t confirm_start = HAL_GetTick();
    uint8_t confirmed = 0;

    while(HAL_GetTick() - confirm_start < RESET_CONFIRM_TIMEOUT) {
        readPins();

        if(RIGHT_PRESSED) {
            confirmed = 1;
            break;
        }

        HAL_Delay(100);
    }

    if(confirmed) {
        // Perform reset
        reset_all_settings();
        ssd1306_InvertDisplay(0);  // Apply default display mode

        // Show success feedback
        ssd1306_Fill(Black);
        ssd1306_SetCursor(35, 20);
        ssd1306_WriteString("RESET OK!", ComicSans_11x12, White);
        ssd1306_UpdateScreen();
        HAL_Delay(1000);

        // Wait for button release
        while(RIGHT_PRESSED) {
            HAL_Delay(10);
            readPins();
        }
    } else {
        // Cancelled
        ssd1306_Fill(Black);
        ssd1306_SetCursor(40, 25);
        ssd1306_WriteString("Cancelled", ComicSans_11x12, White);
        ssd1306_UpdateScreen();
        HAL_Delay(1000);
    }

    // Clear screen
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
}

void handle_credits(){
    // Show confirmation prompt
    ssd1306_SetCursor(5, 10);  // x = 9
    ssd1306_WriteString("Bongo Cat Fidget Toy", ComicSans_11x12, White);
    ssd1306_SetCursor(13, 21);  // x = 3
    ssd1306_WriteString("by Afonso Muralha", ComicSans_11x12, White);
    ssd1306_SetCursor(13, 40);  // x = 9
    ssd1306_WriteString("afonsomuralha.com", ComicSans_11x12, White);

    ssd1306_UpdateScreen();
    // Wait for button release
    while(BOTH_PRESSED || LEFT_PRESSED || RIGHT_PRESSED) {
        HAL_Delay(10);
        readPins();
    }

    while(1) {
        readPins();

        if(RIGHT_PRESSED || LEFT_PRESSED || BOTH_PRESSED) {
            break;
        }

        HAL_Delay(100);
    }

    // Clear screen
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
}

// Handle boot-time button overrides
void handle_boot_overrides(void) {
    readPins();

    // Show credits
    if(BOTH_PRESSED){
        handle_credits();
    }else{
        // If left is pressed at boot, toggle invert from saved state
        if(LEFT_PRESSED) {
            toggle_display_invert();
            force_save();  // Save immediately for boot-time changes
            // Wait for button release
            while(LEFT_PRESSED) {
                HAL_Delay(10);
                readPins();
            }
        }

        // If right is pressed at boot, reset everything
        if(RIGHT_PRESSED) {
            handle_reset();
        }
    }
}

// Update tap speed regularly
void update_tap_speed(void) {
    static uint32_t last_speed_update = 0;
    uint32_t current_time = HAL_GetTick();

    // Update speed calculation every 100ms
    if (current_time - last_speed_update >= 100) {
        calculate_tap_speed();
        last_speed_update = current_time;
    }
}

// Callback: timer has rolled over
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim14)
    {
        readPins();
    }
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM14_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
  ssd1306_Init();

  load_settings();

  // Apply the loaded display invert setting
  ssd1306_InvertDisplay(settings.display_inverted);

  // Initialize save time
  last_save_time = HAL_GetTick();

  // Check button states at boot for override options
  handle_boot_overrides();

  state_e state = IDLE;
  int32_t idle_cntr = 0;
  int32_t tap_left_cntr = 0;
  int32_t tap_right_cntr = 0;
  uint8_t left_state = 0;
  uint8_t right_state = 0;

  HAL_TIM_Base_Start_IT(&htim14);

  while(1) {
    // Periodic tasks
    check_and_save();
    update_saved_indicator();
    update_tap_speed();

    switch(state){
    case IDLE:
        if(sw_state_left == 0 || sw_state_right == 0){
            // Smooth transition - no clear, just go to SWITCH state
            state = SWITCH;
        } else {
            // Idle animation
            draw_animation(ani_idle[idle_cnt]);
            update_display_with_overlays();
            idle_cnt = (idle_cnt + 1) % ani_idle_LEN;
            HAL_Delay(100);
        }
        break;

    case SWITCH:
        // Handle special button combos
        handle_display_mode_switch();
        handle_invert_toggle();

        // Check for idle transition
        if(check_idle_transition(&idle_cntr, &left_state, &right_state)) {
            state = IDLE;
        }
        // Handle normal paw animations
        else if(!NONE_PRESSED) {
            handle_paw_animations(&left_state, &right_state,
                                &tap_left_cntr, &tap_right_cntr, &idle_cntr);
        }

        // Handle tap decay animations
        handle_tap_decay(&tap_left_cntr, &tap_right_cntr);

        // Update display with all overlays
        update_display_with_overlays();
        break;
    }
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */



  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* RCC_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(RCC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(RCC_IRQn);
  /* TIM14_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM14_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM14_IRQn);
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00601133;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_ENABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_DISABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 32-1;      // Was 100-1
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 10000-1;      // 32MHz/32/10000 = 100Hz
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
}


/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC14 PC15 PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA2 PA3
                           PA4 PA5 PA7 PA8
                           PA10 PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_7|GPIO_PIN_8
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : SW_LEFT_Pin */
  GPIO_InitStruct.Pin = SW_LEFT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW_LEFT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SW_RIGHT_Pin */
  GPIO_InitStruct.Pin = SW_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW_RIGHT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB2 PB3 PB4
                           PB5 PB6 PB7 PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : OLED_RST_Pin */
  GPIO_InitStruct.Pin = OLED_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OLED_RST_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
