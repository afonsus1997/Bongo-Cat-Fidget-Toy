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
#include "ssd1306.h"
#include "ssd1306_conf.h"
#include "bongo.h"


I2C_HandleTypeDef I2cHandle;

#define DARA_LENGTH      15                 /* 数据长度 */
#define I2C_ADDRESS      0xA0               /* 本机地址0xA0 */
#define I2C_SPEEDCLOCK   100000             /* 通讯速度100K */
#define I2C_DUTYCYCLE    I2C_DUTYCYCLE_16_9 /* 占空比 */

static void APP_I2cConfig(void);

static void APP_GpioConfig(void);

/**
  * @brief  应用程序入口函数.
  * @retval int
  */
int main(void)
{
  /* 初始化所有外设，Flash接口，SysTick */
  HAL_Init();                                  
  
  /* 初始化GPIO */
  APP_GpioConfig();

  ssd1306_Init();

  while (1)
  {
    /* 延时250ms */
    HAL_Delay(250);   

    /* LED翻转 */
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);    
  }
}

/**
  * @brief  GPIO配置
  * @param  无
  * @retval 无
  */
static void APP_GpioConfig(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct;

  __HAL_RCC_GPIOB_CLK_ENABLE();                          /* 使能GPIOA时钟 */

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;            /* 推挽输出 */
  GPIO_InitStruct.Pull = GPIO_PULLUP;                    /* 使能上拉 */
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;          /* GPIO速度 */  
  /* GPIO初始化 */
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);                
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

/**
  * @brief  错误执行函数
  * @param  无
  * @retval 无
  */
void APP_ErrorHandler(void)
{
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  输出产生断言错误的源文件名及行号
  * @param  file：源文件名指针
  * @param  line：发生断言错误的行号
  * @retval 无
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* 用户可以根据需要添加自己的打印信息,
     例如: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* 无限循环 */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
