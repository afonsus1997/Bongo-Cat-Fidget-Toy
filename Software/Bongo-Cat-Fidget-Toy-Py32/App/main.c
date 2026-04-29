/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "py32f003x6.h"
#include "py32f0xx_hal_gpio.h"
#include "ssd1306.h"
#include "ssd1306_conf.h"
#include "bongo.h"


I2C_HandleTypeDef I2cHandle;

#define I2C_ADDRESS      0xA0
#define I2C_SPEEDCLOCK   400000          /* 400kHz fast mode to match STM32 */
#define I2C_DUTYCYCLE    I2C_DUTYCYCLE_16_9

#define OLED_RST_GPIO_Port GPIOA
#define OLED_RST_Pin GPIO_PIN_12

static void APP_I2cConfig(void);
static void APP_GpioConfig(void);

void run_bongo_loop(void)
{
    state_e state = IDLE;
    uint8_t idle_cnt       = 0;
    uint8_t left_state     = 0;
    uint8_t right_state    = 0;
    int32_t tap_left_cntr  = 0;
    int32_t tap_right_cntr = 0;
    int32_t idle_cntr      = 0;

    uint32_t idle_since      = 0;
    uint32_t sleep_zzz_timer = 0;
    uint8_t  sleep_zzz_frame = 0;

    while (1)
    {
        readPins();
        check_and_save();
        update_saved_indicator();
        update_tap_speed();

        switch (state) {
        case IDLE:
            if (!NONE_PRESSED) {
                if (!ssd1306_GetDisplayOn()) ssd1306_SetDisplayOn(1);
                state      = SWITCH;
                idle_since = 0;
            } else {
                if (idle_since == 0) {
                    idle_since      = HAL_GetTick();
                    sleep_zzz_timer = idle_since;
                    sleep_zzz_frame = 0;
                }

                uint32_t elapsed = HAL_GetTick() - idle_since;

                if (elapsed >= SLEEP_DELAY + DISPLAY_OFF_DELAY) {
                    if (ssd1306_GetDisplayOn()) ssd1306_SetDisplayOn(0);
                } else if (elapsed >= SLEEP_DELAY) {
                    draw_sleep_frame();
                    draw_zzz_overlay(sleep_zzz_frame);
                    update_display_with_overlays();
                    if (HAL_GetTick() - sleep_zzz_timer >= SLEEP_ZZZ_PERIOD) {
                        sleep_zzz_frame = (sleep_zzz_frame + 1) % 4;
                        sleep_zzz_timer = HAL_GetTick();
                    }
                } else {
                    draw_idle_frame(idle_cnt);
                    update_display_with_overlays();
                    idle_cnt = (idle_cnt + 1) % idle_frame_count();
                }

                uint32_t t = HAL_GetTick();
                while (HAL_GetTick() - t < 100) __WFI();
            }
            break;

        case SWITCH:
            // handle_display_mode_switch();

            if (check_idle_transition(&idle_cntr, &left_state, &right_state)) {
                state      = IDLE;
                idle_since = 0;
            } else if (!NONE_PRESSED) {
                handle_paw_animations(
                    &left_state, &right_state,
                    &tap_left_cntr, &tap_right_cntr,
                    &idle_cntr
                );
            }

            if (pending_milestone) {
                play_milestone_celebration(pending_milestone);
                pending_milestone = 0;
                readPins();
                while (!NONE_PRESSED) { readPins(); HAL_Delay(10); }
                left_state     = 0;
                right_state    = 0;
                tap_left_cntr  = 0;
                tap_right_cntr = 0;
                idle_cntr      = 0;
                tap_tracker_reset();
                save_settings();
            }

            handle_tap_decay(&tap_left_cntr, &tap_right_cntr);
            update_display_with_overlays();
            break;
        }
    }
}
  
int main(void)
{
  HAL_Init();
  APP_GpioConfig();
  APP_I2cConfig();

  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  ssd1306_Init();
  ssd1306_SetContrast(DISPLAY_CONTRAST);

  load_settings();
//   ssd1306_InvertDisplay(settings.display_inverted);
  last_save_time = HAL_GetTick();
  tap_tracker_reset();

  handle_boot_overrides();

  run_bongo_loop();
}

static void APP_GpioConfig(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct;

  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* OLED reset */
  GPIO_InitStruct.Pin   = OLED_RST_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OLED_RST_GPIO_Port, &GPIO_InitStruct);

  /* SW_LEFT: PA0, active-high with internal pull-down */
  GPIO_InitStruct.Pin   = SW_LEFT_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  HAL_GPIO_Init(SW_LEFT_GPIO_Port, &GPIO_InitStruct);

  /* SW_RIGHT: PA1, active-high with internal pull-down */
  GPIO_InitStruct.Pin   = SW_RIGHT_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  HAL_GPIO_Init(SW_RIGHT_GPIO_Port, &GPIO_InitStruct);
}

static void APP_I2cConfig(void){
  I2cHandle.Instance             = I2C;                                                                   /* I2C */
  I2cHandle.Init.ClockSpeed      = I2C_SPEEDCLOCK;                                                        /* I2C通讯速度 */
  I2cHandle.Init.DutyCycle       = I2C_DUTYCYCLE;                                                         /* I2C占空比 */
  I2cHandle.Init.OwnAddress1     = I2C_ADDRESS;                                                           /* I2C地址 */
  I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;                                               /* 禁止广播呼叫 */
  I2cHandle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;                                                 /* 允许时钟延长 */
  if (HAL_I2C_Init(&I2cHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

void APP_ErrorHandler(void)
{
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
