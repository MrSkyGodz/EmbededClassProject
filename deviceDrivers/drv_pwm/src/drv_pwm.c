#include "drv_pwm/inc/drv_pwm.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
	TIM_HandleTypeDef* handle;
	TIM_HandleTypeDef ownedHandle;
	DrvPwm_OpenParams_t openParams;
	uint8_t ownsHandle;
	uint8_t gpioInitialized;
} DrvPwm_Context_t;

static DrvPwm_Context_t g_pwmContexts[DRV_PWM_MAX_INSTANCES];

static DrvPwm_Context_t* DrvPwm_GetContext(uint32_t index)
{
	if (index >= DRV_PWM_MAX_INSTANCES)
	{
		return NULL;
	}

	return &g_pwmContexts[index];
}

static uint8_t DrvPwm_IsValidChannel(uint32_t channel)
{
	return (channel == TIM_CHANNEL_1) ||
	       (channel == TIM_CHANNEL_2) ||
	       (channel == TIM_CHANNEL_3) ||
	       (channel == TIM_CHANNEL_4);
}

static HAL_StatusTypeDef DrvPwm_EnableClock(TIM_TypeDef* instance)
{
	if (instance == TIM1)
	{
		__HAL_RCC_TIM1_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM2)
	{
		__HAL_RCC_TIM2_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM3)
	{
		__HAL_RCC_TIM3_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM4)
	{
		__HAL_RCC_TIM4_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM5)
	{
		__HAL_RCC_TIM5_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM6)
	{
		__HAL_RCC_TIM6_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM7)
	{
		__HAL_RCC_TIM7_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM8)
	{
		__HAL_RCC_TIM8_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM9)
	{
		__HAL_RCC_TIM9_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM10)
	{
		__HAL_RCC_TIM10_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM11)
	{
		__HAL_RCC_TIM11_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM12)
	{
		__HAL_RCC_TIM12_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM13)
	{
		__HAL_RCC_TIM13_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == TIM14)
	{
		__HAL_RCC_TIM14_CLK_ENABLE();
		return HAL_OK;
	}

	return HAL_ERROR;
}

static void DrvPwm_DisableClock(TIM_TypeDef* instance)
{
	if (instance == TIM1)
	{
		__HAL_RCC_TIM1_CLK_DISABLE();
	}
	else if (instance == TIM2)
	{
		__HAL_RCC_TIM2_CLK_DISABLE();
	}
	else if (instance == TIM3)
	{
		__HAL_RCC_TIM3_CLK_DISABLE();
	}
	else if (instance == TIM4)
	{
		__HAL_RCC_TIM4_CLK_DISABLE();
	}
	else if (instance == TIM5)
	{
		__HAL_RCC_TIM5_CLK_DISABLE();
	}
	else if (instance == TIM6)
	{
		__HAL_RCC_TIM6_CLK_DISABLE();
	}
	else if (instance == TIM7)
	{
		__HAL_RCC_TIM7_CLK_DISABLE();
	}
	else if (instance == TIM8)
	{
		__HAL_RCC_TIM8_CLK_DISABLE();
	}
	else if (instance == TIM9)
	{
		__HAL_RCC_TIM9_CLK_DISABLE();
	}
	else if (instance == TIM10)
	{
		__HAL_RCC_TIM10_CLK_DISABLE();
	}
	else if (instance == TIM11)
	{
		__HAL_RCC_TIM11_CLK_DISABLE();
	}
	else if (instance == TIM12)
	{
		__HAL_RCC_TIM12_CLK_DISABLE();
	}
	else if (instance == TIM13)
	{
		__HAL_RCC_TIM13_CLK_DISABLE();
	}
	else if (instance == TIM14)
	{
		__HAL_RCC_TIM14_CLK_DISABLE();
	}
}

static HAL_StatusTypeDef DrvPwm_PreparePin(const DrvGpio_OpenParams_t* source,
                                           DrvGpio_OpenParams_t* prepared)
{
	if ((source == NULL) || (prepared == NULL) || (source->Port == NULL) || (source->Pin == 0U))
	{
		return HAL_ERROR;
	}

	*prepared = *source;
	prepared->Mode = GPIO_MODE_AF_PP;
	prepared->Pull = GPIO_NOPULL;
	prepared->Speed = GPIO_SPEED_FREQ_LOW;
	return HAL_OK;
}

static HAL_StatusTypeDef DrvPwm_InitPin(DrvPwm_Context_t* context)
{
	DrvGpio_OpenParams_t pwmPin = {0};

	if ((context == NULL) || (context->openParams.InitGpio == 0U))
	{
		return HAL_OK;
	}

	if (DrvPwm_PreparePin(&context->openParams.GPIO_Init, &pwmPin) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (DrvGpio_Open(&pwmPin) != HAL_OK)
	{
		return HAL_ERROR;
	}

	context->gpioInitialized = 1U;
	return HAL_OK;
}

static void DrvPwm_FillChannelConfig(const DrvPwm_ChannelInit_t* source,
                                     TIM_OC_InitTypeDef* config)
{
	memset(config, 0, sizeof(*config));
	config->OCMode = source->OCMode;
	config->Pulse = source->Pulse;
	config->OCPolarity = source->OCPolarity;
	config->OCFastMode = source->OCFastMode;
}

static HAL_StatusTypeDef DrvPwm_ConfigChannel(DrvPwm_Context_t* context)
{
	TIM_OC_InitTypeDef config = {0};

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	DrvPwm_FillChannelConfig(&context->openParams.ChannelInit, &config);
	return HAL_TIM_PWM_ConfigChannel(context->handle, &config, context->openParams.Channel);
}

static HAL_StatusTypeDef DrvPwm_InitOwnedHandle(DrvPwm_Context_t* context)
{
	TIM_ClockConfigTypeDef clockSourceConfig = {0};

	if ((context == NULL) || (context->openParams.PWM_Init.Instance == NULL))
	{
		return HAL_ERROR;
	}

	memset(&context->ownedHandle, 0, sizeof(context->ownedHandle));
	context->ownedHandle.Instance = context->openParams.PWM_Init.Instance;
	context->ownedHandle.Init.Prescaler = context->openParams.PWM_Init.Prescaler;
	context->ownedHandle.Init.CounterMode = context->openParams.PWM_Init.CounterMode;
	context->ownedHandle.Init.Period = context->openParams.PWM_Init.Period;
	context->ownedHandle.Init.ClockDivision = context->openParams.PWM_Init.ClockDivision;
	context->ownedHandle.Init.AutoReloadPreload = context->openParams.PWM_Init.AutoReloadPreload;

	context->handle = &context->ownedHandle;
	context->ownsHandle = 1U;

	if (DrvPwm_EnableClock(context->openParams.PWM_Init.Instance) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (DrvPwm_InitPin(context) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (HAL_TIM_Base_Init(context->handle) != HAL_OK)
	{
		return HAL_ERROR;
	}

	clockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(context->handle, &clockSourceConfig) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (HAL_TIM_PWM_Init(context->handle) != HAL_OK)
	{
		return HAL_ERROR;
	}

	return DrvPwm_ConfigChannel(context);
}

HAL_StatusTypeDef DrvPwm_Open(void* vpParam)
{
	const DrvPwm_OpenParams_t* openParams = (const DrvPwm_OpenParams_t*) vpParam;
	DrvPwm_Context_t* context = NULL;

	if ((openParams == NULL) ||
	    (openParams->PwmIndex >= DRV_PWM_MAX_INSTANCES) ||
	    (DrvPwm_IsValidChannel(openParams->Channel) == 0U))
	{
		return HAL_ERROR;
	}

	context = DrvPwm_GetContext(openParams->PwmIndex);
	if (context == NULL)
	{
		return HAL_ERROR;
	}

	memset(context, 0, sizeof(*context));
	context->openParams = *openParams;

	if (openParams->TIMHandle != NULL)
	{
		context->handle = openParams->TIMHandle;

		if (DrvPwm_InitPin(context) != HAL_OK)
		{
			memset(context, 0, sizeof(*context));
			return HAL_ERROR;
		}

		return DrvPwm_ConfigChannel(context);
	}

	if (DrvPwm_InitOwnedHandle(context) != HAL_OK)
	{
		memset(context, 0, sizeof(*context));
		return HAL_ERROR;
	}

	return HAL_OK;
}

HAL_StatusTypeDef DrvPwm_Close(void* vpParam)
{
	const DrvPwm_DeviceParams_t* deviceParams = (const DrvPwm_DeviceParams_t*) vpParam;
	DrvPwm_Context_t* context = NULL;

	if (deviceParams == NULL)
	{
		return HAL_ERROR;
	}

	context = DrvPwm_GetContext(deviceParams->PwmIndex);
	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	(void) HAL_TIM_PWM_Stop(context->handle, context->openParams.Channel);

	if (context->ownsHandle != 0U)
	{
		(void) HAL_TIM_PWM_DeInit(context->handle);
		DrvPwm_DisableClock(context->handle->Instance);
	}

	if (context->gpioInitialized != 0U)
	{
		(void) DrvGpio_Close(&context->openParams.GPIO_Init);
	}

	memset(context, 0, sizeof(*context));
	return HAL_OK;
}

TIM_HandleTypeDef* DrvPwm_GetHandle(uint32_t index)
{
	DrvPwm_Context_t* context = DrvPwm_GetContext(index);

	if (context == NULL)
	{
		return NULL;
	}

	return context->handle;
}

HAL_StatusTypeDef DrvPwm_Start(uint32_t index)
{
	DrvPwm_Context_t* context = DrvPwm_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	return HAL_TIM_PWM_Start(context->handle, context->openParams.Channel);
}

HAL_StatusTypeDef DrvPwm_Stop(uint32_t index)
{
	DrvPwm_Context_t* context = DrvPwm_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	return HAL_TIM_PWM_Stop(context->handle, context->openParams.Channel);
}

HAL_StatusTypeDef DrvPwm_SetCompare(void* vpParam)
{
	const DrvPwm_CompareRequest_t* request = (const DrvPwm_CompareRequest_t*) vpParam;
	DrvPwm_Context_t* context = NULL;
	uint32_t period = 0U;
	uint32_t compare = 0U;

	if (request == NULL)
	{
		return HAL_ERROR;
	}

	context = DrvPwm_GetContext(request->PwmIndex);
	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	period = __HAL_TIM_GET_AUTORELOAD(context->handle);
	compare = request->Compare;
	if (compare > period)
	{
		compare = period;
	}

	__HAL_TIM_SET_COMPARE(context->handle, context->openParams.Channel, compare);
	return HAL_OK;
}

HAL_StatusTypeDef DrvPwm_SetDuty(void* vpParam)
{
	const DrvPwm_DutyRequest_t* request = (const DrvPwm_DutyRequest_t*) vpParam;
	DrvPwm_Context_t* context = NULL;
	uint32_t period = 0U;
	uint32_t duty = 0U;
	uint32_t compare = 0U;
	DrvPwm_CompareRequest_t compareRequest = {0};

	if ((request == NULL) || (request->DutyMax == 0U))
	{
		return HAL_ERROR;
	}

	context = DrvPwm_GetContext(request->PwmIndex);
	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	period = __HAL_TIM_GET_AUTORELOAD(context->handle);
	duty = request->Duty;
	if (duty > request->DutyMax)
	{
		duty = request->DutyMax;
	}

	compare = (uint32_t) ((((uint64_t) period * duty) + (request->DutyMax / 2U)) / request->DutyMax);

	compareRequest.PwmIndex = request->PwmIndex;
	compareRequest.Compare = compare;
	return DrvPwm_SetCompare(&compareRequest);
}

HAL_StatusTypeDef DrvPwm_Ioctl(const uint32_t command, void* vpParam)
{
	const DrvPwm_DeviceParams_t* deviceParams = (const DrvPwm_DeviceParams_t*) vpParam;

	switch ((DrvPwm_IoctlCmd_t) command)
	{
	case DRV_PWM_IOCTL_START:
		if (deviceParams == NULL)
		{
			return HAL_ERROR;
		}
		return DrvPwm_Start(deviceParams->PwmIndex);

	case DRV_PWM_IOCTL_STOP:
		if (deviceParams == NULL)
		{
			return HAL_ERROR;
		}
		return DrvPwm_Stop(deviceParams->PwmIndex);

	case DRV_PWM_IOCTL_SET_COMPARE:
		return DrvPwm_SetCompare(vpParam);

	case DRV_PWM_IOCTL_SET_DUTY:
		return DrvPwm_SetDuty(vpParam);

	default:
		return HAL_ERROR;
	}
}
