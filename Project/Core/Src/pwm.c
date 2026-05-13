/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pwm.c
  * @brief   PWM glue layer backed by drv_pwm.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "pwm.h"

static DrvPwm_OpenParams_t g_ledRedPwmOpenParams =
{
	.PwmIndex = LED_RED_PWM_INDEX,
	.PWM_Init = {
		.Instance = LED_RED_PWM_INSTANCE,
		.Prescaler = 0U,
		.CounterMode = TIM_COUNTERMODE_UP,
		.Period = 65535U,
		.ClockDivision = TIM_CLOCKDIVISION_DIV1,
		.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE,
	},
	.ChannelInit = {
		.OCMode = TIM_OCMODE_PWM1,
		.Pulse = 0U,
		.OCPolarity = TIM_OCPOLARITY_HIGH,
		.OCFastMode = TIM_OCFAST_DISABLE,
	},
	.GPIO_Init = {
		.GPIOIndex = LED_RED_INDEX,
		.Port = LED_RED_GPIO_Port,
		.Pin = LED_RED_Pin,
		.Function = LED_RED_PWM_GPIO_AF,
	},
	.Channel = LED_RED_PWM_CHANNEL,
	.TIMHandle = NULL,
	.InitGpio = 1U,
};

HAL_StatusTypeDef PWM_InitWithParams(const DrvPwm_OpenParams_t* openParams)
{
	DrvPwm_DeviceParams_t deviceParams = {0};

	if (openParams == NULL)
	{
		return HAL_ERROR;
	}

	deviceParams.PwmIndex = openParams->PwmIndex;

	(void) DrvPwm_Close(&deviceParams);

	if (DrvPwm_Open((void*) openParams) != HAL_OK)
	{
		return HAL_ERROR;
	}

	return DrvPwm_Start(openParams->PwmIndex);
}

void PWM_Init(void)
{
	if (PWM_InitWithParams(&g_ledRedPwmOpenParams) != HAL_OK)
	{
		Error_Handler();
	}
}

HAL_StatusTypeDef PWM_SetDuty(uint32_t pwmIndex, uint32_t duty, uint32_t dutyMax)
{
	DrvPwm_DutyRequest_t pwmDutyRequest =
	{
		.PwmIndex = pwmIndex,
		.Duty = duty,
		.DutyMax = dutyMax,
	};

	return DrvPwm_SetDuty(&pwmDutyRequest);
}

HAL_StatusTypeDef PWM_SetLedRedDuty(uint32_t duty)
{
	return PWM_SetDuty(LED_RED_PWM_INDEX, duty, LED_RED_PWM_DUTY_MAX);
}
