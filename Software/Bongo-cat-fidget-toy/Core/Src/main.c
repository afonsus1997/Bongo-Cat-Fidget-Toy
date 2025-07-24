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
#include <stdio.h>  // For sprintf

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IDLE_TIME 2000
#define TAP_DECAY_TIME 200

#define LEFT_PRESSED (sw_state_left == 0 && sw_state_right == 1)
#define RIGHT_PRESSED (sw_state_left == 1 && sw_state_right == 0)
#define BOTH_PRESSED (sw_state_left == 0 && sw_state_right == 0)
#define NONE_PRESSED (sw_state_left == 1 && sw_state_right == 1)

// Flash memory addresses for storing tap counts and settings
#define FLASH_USER_START_ADDR   0x08007800   // Last 2KB of 32KB flash
#define FLASH_USER_END_ADDR     0x08007FFF
// Correct addresses for 64KB STM32G030K8
#define FLASH_SIZE              0x10000      // 64KB
#define FLASH_PAGE_SIZE         0x800        // 2KB per page
#define FLASH_PAGE_NB           32           // 64KB / 2KB = 32 pages (0-31)

// Use the last page (page 31) for storage
#define FLASH_USER_START_ADDR   0x0800F800   // Start of last page (page 31)
#define FLASH_USER_END_ADDR     0x0800FFFF   // End of flash


// Flash data offsets
#define FLASH_OFFSET_TOTAL_TAPS    0
#define FLASH_OFFSET_LEFT_TAPS     8
#define FLASH_OFFSET_RIGHT_TAPS    16
#define FLASH_OFFSET_DISPLAY_INV   24

// Flash save timing
#define FLASH_SAVE_INTERVAL 30000  // Save every 30 seconds (30000ms)
#define SAVED_DISPLAY_TIME 2000     // Show "saved!" for 2 seconds
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

// Display settings
uint8_t display_inverted = 0;

// Tap counters
uint32_t total_taps = 0;
uint32_t left_taps = 0;
uint32_t right_taps = 0;

// Timer for display mode
uint32_t both_pressed_timer = 0;
uint8_t display_mode = 0;  // 0 = normal animation, 1 = show tap count overlay
#define MODE_SWITCH_TIME 3000  // 3 seconds to switch display mode

// Timer for invert toggle
uint32_t invert_timer = 0;
#define INVERT_HOLD_TIME 2000  // 2 seconds to toggle invert

// Flash save management
uint32_t last_save_time = 0;
uint8_t data_changed = 0;  // Flag to track if data needs saving

// Saved indicator
uint32_t saved_indicator_timer = 0;
uint8_t show_saved_indicator = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM14_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

// timer value = desired_sec * 64e6/prescaler

void toggle_display_invert(void) {
    display_inverted = !display_inverted;
    ssd1306_InvertDisplay(display_inverted);

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

    // Semi-transparent background box for readability
//    ssd1306_FillRectangle(0, 0, 127, 30, White);
//    ssd1306_FillRectangle(2, 2, 125, 28, Black);

    // Display title
    ssd1306_SetCursor(25, 3);
    ssd1306_WriteString("TAP COUNT", ComicSans_11x12, White);

    // Display counts in a single line to save space
    sprintf(buffer, "T:%lu L:%lu R:%lu", total_taps, left_taps, right_taps);
    ssd1306_SetCursor(10, 15);
    ssd1306_WriteString(buffer, ComicSans_11x12, White);
}

// Display saved indicator
void display_saved_indicator(void) {
    // Display "saved!" in bottom right corner
    ssd1306_SetCursor(85, 54);
    ssd1306_WriteString("saved!", ComicSans_11x12, White);
}

// Save all settings to flash
void save_settings(void) {

	HAL_FLASH_Unlock();

	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t PageError;

	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.Page = 31;  // Last page for 64KB device (not 15!)
	EraseInitStruct.NbPages = 1;

	HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    // Write counters and settings
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                     FLASH_USER_START_ADDR + FLASH_OFFSET_TOTAL_TAPS,
                     total_taps);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                     FLASH_USER_START_ADDR + FLASH_OFFSET_LEFT_TAPS,
                     left_taps);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                     FLASH_USER_START_ADDR + FLASH_OFFSET_RIGHT_TAPS,
                     right_taps);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                     FLASH_USER_START_ADDR + FLASH_OFFSET_DISPLAY_INV,
                     (uint64_t)display_inverted);

    HAL_FLASH_Lock();

    // Update last save time
    last_save_time = HAL_GetTick();
    data_changed = 0;  // Clear the changed flag

    // Trigger saved indicator
    show_saved_indicator = 1;
    saved_indicator_timer = HAL_GetTick();
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

// Load all settings from flash
void load_settings(void) {
    // Load tap counters
    total_taps = *(__IO uint32_t*)(FLASH_USER_START_ADDR + FLASH_OFFSET_TOTAL_TAPS);
    left_taps = *(__IO uint32_t*)(FLASH_USER_START_ADDR + FLASH_OFFSET_LEFT_TAPS);
    right_taps = *(__IO uint32_t*)(FLASH_USER_START_ADDR + FLASH_OFFSET_RIGHT_TAPS);

    // Load display invert setting
    uint32_t invert_setting = *(__IO uint32_t*)(FLASH_USER_START_ADDR + FLASH_OFFSET_DISPLAY_INV);

    // Check for valid data (not 0xFFFFFFFF)
    if(total_taps == 0xFFFFFFFF) total_taps = 0;
    if(left_taps == 0xFFFFFFFF) left_taps = 0;
    if(right_taps == 0xFFFFFFFF) right_taps = 0;

    // For display_inverted, only check the first byte
    if((invert_setting & 0xFF) != 0xFF) {
        display_inverted = (uint8_t)(invert_setting & 0x01);
    } else {
        display_inverted = 0;  // Default to normal display
    }
}

// Reset all counters and settings
void reset_all_settings(void) {
    // Don't do flash operations too early
    if (HAL_GetTick() < 100) {
        HAL_Delay(100);  // Ensure system is stable
    }

    // Set values to defaults
    total_taps = 0;
    left_taps = 0;
    right_taps = 0;
    display_inverted = 0;

    // Add error checking
    save_settings();
}

// Callback: timer has rolled over
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
//  // Check which version of the timer triggered this callback and toggle LED
    if (htim == &htim14 )
    {
        readPins();
    }
}

typedef enum states {
    IDLE,
    SWITCH,
} state_e;

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

//  display_tap_count_overlay();

  // Load all settings from flash (including display invert)
  load_settings();

  // Apply the loaded display invert setting
  ssd1306_InvertDisplay(display_inverted);

  // Initialize save time
  last_save_time = HAL_GetTick();

  // Check button states at boot for override options
  readPins();

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
      reset_all_settings();
      ssd1306_InvertDisplay(0);  // Apply default display mode

      // Show feedback
      ssd1306_Fill(White);
      ssd1306_UpdateScreen();
      HAL_Delay(200);
      ssd1306_Fill(Black);
      ssd1306_UpdateScreen();
      HAL_Delay(200);

      // Wait for button release
      while(RIGHT_PRESSED) {
          HAL_Delay(10);
          readPins();
      }
  }

  state_e state = IDLE;
  int32_t idle_cntr = 0;
  int32_t tap_left_cntr = 0;
  int32_t tap_right_cntr = 0;
  uint8_t left_state = 0;
  uint8_t right_state = 0;

  HAL_TIM_Base_Start_IT(&htim14);
  while(1) {
    // Check if it's time to save to flash
    check_and_save();

    // Check if we should hide the saved indicator
    if (show_saved_indicator && (HAL_GetTick() - saved_indicator_timer >= SAVED_DISPLAY_TIME)) {
        show_saved_indicator = 0;
    }

    switch(state){
    case IDLE:
        if(sw_state_left == 0 || sw_state_right == 0){
            draw_animation(&img_both_up);
            ssd1306_UpdateScreen();
            HAL_Delay(50);
            state = SWITCH;
        }else {
            draw_animation(ani_idle[idle_cnt]);

            // Draw overlay on top if enabled
            if (display_mode) {
                display_tap_count_overlay();
            }

            // Show saved indicator if active
            if (show_saved_indicator) {
                display_saved_indicator();
            }

            ssd1306_UpdateScreen();
            idle_cnt = (idle_cnt + 1 ) % ani_idle_LEN;
            HAL_Delay(100);
        }
        break;
    case SWITCH:
        // Check for display mode switch (both buttons held for 3 seconds)
        if (BOTH_PRESSED) {
            if (both_pressed_timer == 0) {
                both_pressed_timer = HAL_GetTick();
            } else if (HAL_GetTick() - both_pressed_timer >= MODE_SWITCH_TIME) {
                display_mode = !display_mode;
                both_pressed_timer = 0;

                // Force save when switching modes
                force_save();

                // Wait for button release
                while(BOTH_PRESSED) {
                    HAL_Delay(10);
                    readPins();
                }
            }
        } else {
            both_pressed_timer = 0;
        }

        // Check for invert toggle (hold left for 2 seconds) - works with or without overlay
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
            }
        } else {
            invert_timer = 0;
        }

        // Normal animation routines continue regardless of overlay
        // Idle reset routines
        if(NONE_PRESSED){
            draw_animation(&img_both_up);
            if(idle_cntr == 0){
                idle_cntr = HAL_GetTick();
            }
            if(HAL_GetTick() - idle_cntr >= IDLE_TIME){
                idle_cntr = 0;
                // Force save before going to idle
                force_save();
                state = IDLE;
            }
            if(left_state)
                left_state = 0;
            if(right_state)
                right_state = 0;
        }
        // Paw draw routines
        else {
            idle_cntr = 0;
            if((BOTH_PRESSED) && ((left_state | right_state == 0) || (left_state ^ right_state == 1))){
                draw_animation(&img_both_down_alt);
                if(!right_state){
                    draw_animation_transparent(&img_tap_right);
                    tap_right_cntr = HAL_GetTick();
                    right_taps++;  // Increment right tap counter
                    total_taps++;   // Increment total tap counter
                    data_changed = 1;  // Mark data as changed
                }
                if(!left_state){
                    draw_animation_transparent(&img_tap_left);
                    tap_left_cntr = HAL_GetTick();
                    left_taps++;    // Increment left tap counter
                    total_taps++;   // Increment total tap counter
                    data_changed = 1;  // Mark data as changed
                }
                right_state = 1; left_state = 1;
            }
            if(RIGHT_PRESSED){
                if(right_state == 0 || left_state == 1){
                    draw_animation(&img_right_down_alt);
                    if(!right_state){
                        draw_animation_transparent(&img_tap_right);
                        tap_right_cntr = HAL_GetTick();
                        right_taps++;  // Increment right tap counter
                        total_taps++;   // Increment total tap counter
                        data_changed = 1;  // Mark data as changed
                    }
                    right_state = 1;
                }
                if(left_state)
                    left_state = 0;
            }
            if(LEFT_PRESSED){
                if(left_state == 0 || right_state == 1){
                    draw_animation(&img_left_down_alt);
                    if(!left_state){
                        draw_animation_transparent(&img_tap_left);
                        tap_left_cntr = HAL_GetTick();
                        left_taps++;    // Increment left tap counter
                        total_taps++;   // Increment total tap counter
                        data_changed = 1;  // Mark data as changed
                    }
                    left_state = 1;
                }
                if(right_state)
                    right_state = 0;
            }
        }

        // Tap decay routines
        if(tap_left_cntr > 0){
            if(HAL_GetTick() - tap_left_cntr > TAP_DECAY_TIME) {
                draw_animation_erase(&img_tap_left);
                tap_left_cntr = 0;
            }
            else{
                draw_animation_transparent(&img_tap_left);
            }
        }
        if(tap_right_cntr > 0){
            if(HAL_GetTick() - tap_right_cntr > TAP_DECAY_TIME) {
                draw_animation_erase(&img_tap_right);
                tap_right_cntr = 0;
            }
            else{
                draw_animation_transparent(&img_tap_right);
            }
        }

        // Draw overlay on top if enabled
        if (display_mode) {
            display_tap_count_overlay();
        }

        ssd1306_UpdateScreen();
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
