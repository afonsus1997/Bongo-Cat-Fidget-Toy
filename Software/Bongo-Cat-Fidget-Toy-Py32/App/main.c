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
#include "py32f030x6.h"
#include "py32f0xx_hal_gpio.h"
#include "ssd1306.h"
#include "ssd1306_conf.h"
#include "bongo.h"


I2C_HandleTypeDef I2cHandle;

#define DARA_LENGTH      15                 
#define I2C_ADDRESS      0xA0               
#define I2C_SPEEDCLOCK   100000             
#define I2C_DUTYCYCLE    I2C_DUTYCYCLE_16_9 

#define OLED_RST_GPIO_Port GPIOA
#define OLED_RST_Pin GPIO_PIN_12

static void APP_I2cConfig(void);

static void APP_GpioConfig(void);

int I2C_ScanBus(void);


  
int main(void)
{
  HAL_Init();                                  
  APP_GpioConfig();
  APP_I2cConfig();

  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(100);
  ssd1306_Init();
  
  

  while (HAL_I2C_GetState(&I2cHandle) != HAL_I2C_STATE_READY);
  volatile int status = 0;
  status = I2C_ScanBus();

  ssd1306_Init();
  ssd1306_Line(0,0,128,32,0x01);
  ssd1306_UpdateScreen();

  while (1)
  {
    HAL_Delay(250);   
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);    
  }
}

static void APP_GpioConfig(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct;

  __HAL_RCC_GPIOB_CLK_ENABLE();                          
  __HAL_RCC_GPIOA_CLK_ENABLE();                          

  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OLED_RST_GPIO_Port, &GPIO_InitStruct);

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

int I2C_ScanBus(void)
{
  HAL_StatusTypeDef result;
  uint8_t i;
  char msg[32];

  for (i = 1; i < 128; i++)
  {
    uint8_t address = i << 1; // 7-bit address shifted left (HAL expects 8-bit)
    result = HAL_I2C_IsDeviceReady(&I2cHandle, address, 2, 10);

    if (result == HAL_OK)
    {
      // Device found
      return 1;
    }
    else
    {
      // No ACK received
      HAL_Delay(2);
    }
  }
  return 0;
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
