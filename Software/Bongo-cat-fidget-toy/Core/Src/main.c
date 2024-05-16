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
#include "ssd1306_tests.h"
#include "bitmap_extended.h"

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
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim14;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM14_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

// timer value = desired_sec * 64e6/prescaler
int sw_state_left;
int sw_state_right;

uint8_t idle_cnt;


void draw_animation(char* frame){
	ssd1306_Fill(Black);
	ssd1306_DrawBitmap(0,0,frame,128,64,White);
//	ssd1306_UpdateScreen();
}

void draw_animation_erase(char* frame){
	ssd1306_DrawBitmap(0,0,frame,128,64,Black);
//	ssd1306_UpdateScreen();
}

void draw_animation_transparent(char* frame){
	ssd1306_DrawBitmap(0,0,frame,128,64,White);
//	ssd1306_UpdateScreen();
}

// Callback: timer has rolled over
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
//  // Check which version of the timer triggered this callback and toggle LED
	if (htim == &htim14 )
	{
		sw_state_left = HAL_GPIO_ReadPin(SW_LEFT_GPIO_Port, SW_LEFT_Pin);
		sw_state_right = HAL_GPIO_ReadPin(SW_RIGHT_GPIO_Port, SW_RIGHT_Pin);
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
  ssd1306_Init();
//    ssd1306_TestAll();

  state_e state = IDLE;
  int32_t idle_cntr = 0;
  int32_t tap_left_cntr = 0;
  int32_t tap_right_cntr = 0;
  uint8_t left_state = 0;
  uint8_t right_state = 0;

  HAL_TIM_Base_Start_IT(&htim14);
  while(1) {
	switch(state){
	case IDLE:
		if(sw_state_left == 0 || sw_state_right == 0){
			draw_animation(&img_both_up);
			ssd1306_UpdateScreen();
			HAL_Delay(50);
			state = SWITCH;
		}else {
			  draw_animation(ani_idle[idle_cnt]);
			  ssd1306_UpdateScreen();
			  idle_cnt = (idle_cnt + 1 ) % ani_idle_LEN;
			  HAL_Delay(100);
		}
		break;
	case SWITCH:
		// Idle reset routines
		if(NONE_PRESSED){
			draw_animation(&img_both_up);
			if(idle_cntr == 0){
				idle_cntr = HAL_GetTick();
			}
			if(HAL_GetTick() - idle_cntr >= IDLE_TIME){
				idle_cntr = 0;
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
				}
				if(!left_state){
					draw_animation_transparent(&img_tap_left);
					tap_left_cntr = HAL_GetTick();
				}
				right_state = 1; left_state = 1;
			}
			if(RIGHT_PRESSED){
				if(right_state == 0 || left_state == 1){
					draw_animation(&img_right_down_alt);
					if(!right_state){
						draw_animation_transparent(&img_tap_right);
						tap_right_cntr = HAL_GetTick();
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
					}
					left_state = 1;

				}
				if(right_state)
					right_state = 0;
			}


		}
		// Tap animation routines
//		if(sw_state_left == 0 && left_state == 0 && tap_left_cntr == 0){
//			draw_animation_transparent(&img_tap_left);
//			tap_left_cntr = HAL_GetTick();
//		}
//		if(sw_state_right == 0 && right_state == 0 && tap_right_cntr == 0){
//			draw_animation_transparent(&img_tap_right);
//			tap_right_cntr = HAL_GetTick();
//		}

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

//		HAL_Delay(100);
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
  hi2c1.Init.Timing = 0x00300B29;
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
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }

  /** I2C Fast mode Plus enable
  */
  HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C1);
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

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 100-1;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 6400-1;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

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

  /*Configure GPIO pins : PC14 PC15 PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA2 PA3
                           PA4 PA5 PA7 PA8
                           PA10 PA11 PA12 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_7|GPIO_PIN_8
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_15;
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

#ifdef  USE_FULL_ASSERT
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
