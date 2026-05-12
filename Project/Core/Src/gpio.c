/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "drv_gpio/inc/drv_gpio.h"
#include "board.h"


DrvGpio_OpenParams_t GpioButtonOpenParams =
{
	.GPIOIndex = BUTTON_GPIO_INDEX,
	.Port = BUTTON_GPIO_GPIO_Port,
	.Pin = BUTTON_GPIO_Pin,
	.Mode = GPIO_MODE_IT_RISING,
	.Pull = GPIO_NOPULL,
	.InitInterrupt = {
			.InterruptEnabled = 1,
			.InterruptIRQn = BUTTON_GPIO_EXTI_IRQn,
			.InterruptPriority = 6,
			.InterruptSubPriority = 0,
	},
};


DrvGpio_OpenParams_t GpioLedRedOpenParams =
{
	.GPIOIndex = LED_RED_INDEX,
	.Port = LED_RED_GPIO_Port,
	.Pin = LED_RED_Pin,
	.Pull = GPIO_NOPULL,
	.Speed = GPIO_SPEED_FREQ_LOW,
	.Mode = GPIO_MODE_AF_PP,
	.Function = GPIO_AF9_TIM12,
};



DrvGpio_OpenParams_t GpioLedBlueOpenParams =
{
	.GPIOIndex = LED_BLUE_INDEX,
	.Port = LED_BLUE_GPIO_Port,
	.Pin = LED_BLUE_Pin,
	.Mode = GPIO_MODE_OUTPUT_PP,
	.Pull = GPIO_NOPULL,
	.Speed = GPIO_SPEED_FREQ_LOW,
};


void GPIO_Init(void)
{
//	DrvGpio_Open(&GpioButtonOpenParams);
	DrvGpio_Open(&GpioLedBlueOpenParams);

//	DrvGpio_Write(LED_BLUE_INDEX, GPIO_PIN_SET);
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
