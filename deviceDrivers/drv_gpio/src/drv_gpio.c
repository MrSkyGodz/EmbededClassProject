#include "drv_gpio/inc/drv_gpio.h"

#include <string.h>

typedef struct
{
	DrvGpio_OpenParams_t OpenParams;
	DrvGpio_InterruptCallback_t Callback;
	void* UserData;
	uint8_t IsOpen;
} DrvGpio_Context_t;

static DrvGpio_Context_t g_gpioContexts[DRV_GPIO_MAX_INSTANCES];

static DrvGpio_Context_t* DrvGpio_GetContext(uint32_t gpioIndex)
{
	if (gpioIndex >= DRV_GPIO_MAX_INSTANCES)
	{
		return NULL;
	}

	return &g_gpioContexts[gpioIndex];
}

static HAL_StatusTypeDef DrvGpio_EnableClock(GPIO_TypeDef* port)
{
	if (port == GPIOA)
	{
		__HAL_RCC_GPIOA_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOB)
	{
		__HAL_RCC_GPIOB_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOC)
	{
		__HAL_RCC_GPIOC_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOD)
	{
		__HAL_RCC_GPIOD_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOE)
	{
		__HAL_RCC_GPIOE_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOF)
	{
		__HAL_RCC_GPIOF_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOG)
	{
		__HAL_RCC_GPIOG_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOH)
	{
		__HAL_RCC_GPIOH_CLK_ENABLE();
		return HAL_OK;
	}
	if (port == GPIOI)
	{
		__HAL_RCC_GPIOI_CLK_ENABLE();
		return HAL_OK;
	}
#ifdef GPIOJ
	if (port == GPIOJ)
	{
		__HAL_RCC_GPIOJ_CLK_ENABLE();
		return HAL_OK;
	}
#endif
#ifdef GPIOK
	if (port == GPIOK)
	{
		__HAL_RCC_GPIOK_CLK_ENABLE();
		return HAL_OK;
	}
#endif

	return HAL_ERROR;
}

static uint8_t DrvGpio_ModeUsesInterrupt(uint32_t mode)
{
	return (mode == GPIO_MODE_IT_RISING) ||
	       (mode == GPIO_MODE_IT_FALLING) ||
	       (mode == GPIO_MODE_IT_RISING_FALLING);
}

static HAL_StatusTypeDef DrvGpio_InitInterrupt(const DrvGpio_InterruptInit_t* InterruptInit)
{
	if ((InterruptInit == NULL) || (InterruptInit->InterruptEnabled == 0U))
	{
		return HAL_OK;
	}

	HAL_NVIC_SetPriority(InterruptInit->InterruptIRQn,
						 InterruptInit->InterruptPriority,
						 InterruptInit->InterruptSubPriority);
	HAL_NVIC_EnableIRQ(InterruptInit->InterruptIRQn);
	return HAL_OK;
}

HAL_StatusTypeDef DrvGpio_Open(void* vpParam)
{
	const DrvGpio_OpenParams_t* openParams = (const DrvGpio_OpenParams_t*) vpParam;
	DrvGpio_Context_t* context = NULL;
	GPIO_InitTypeDef gpioConfig = {0};

	if ((openParams == NULL) || (openParams->Port == NULL) || (openParams->Pin == 0U))
	{
		return HAL_ERROR;
	}

	context = DrvGpio_GetContext(openParams->GPIOIndex);
	if (context == NULL)
	{
		return HAL_ERROR;
	}

	if (DrvGpio_EnableClock(openParams->Port) != HAL_OK)
	{
		return HAL_ERROR;
	}

	gpioConfig.Pin = openParams->Pin;
	gpioConfig.Mode = openParams->Mode;
	gpioConfig.Pull = openParams->Pull;
	gpioConfig.Speed = openParams->Speed;
	gpioConfig.Alternate = openParams->Function;
	HAL_GPIO_Init(openParams->Port, &gpioConfig);

	memset(context, 0, sizeof(*context));
	context->OpenParams = *openParams;
	context->IsOpen = 1U;

	return DrvGpio_InitInterrupt(&openParams->InitInterrupt);
}

HAL_StatusTypeDef DrvGpio_Close(void* vpParam)
{
	const DrvGpio_OpenParams_t* openParams = (const DrvGpio_OpenParams_t*) vpParam;
	DrvGpio_Context_t* context = NULL;

	if (openParams == NULL)
	{
		return HAL_ERROR;
	}

	context = DrvGpio_GetContext(openParams->GPIOIndex);
	if ((context == NULL) || (context->IsOpen == 0U))
	{
		return HAL_ERROR;
	}

	HAL_GPIO_DeInit(context->OpenParams.Port, context->OpenParams.Pin);
	memset(context, 0, sizeof(*context));
	return HAL_OK;
}

GPIO_PinState DrvGpio_Read(uint16_t gpioIndex)
{
	DrvGpio_Context_t* Context = DrvGpio_GetContext(gpioIndex);

	if ((Context->IsOpen ==  0) || (Context->OpenParams.Port == NULL))
	{
		return HAL_ERROR;
	}

	return HAL_GPIO_ReadPin(Context->OpenParams.Port, Context->OpenParams.Pin);
}

HAL_StatusTypeDef DrvGpio_Write(uint16_t gpioIndex, GPIO_PinState state)
{
	DrvGpio_Context_t* Context = DrvGpio_GetContext(gpioIndex);

	if ((Context->IsOpen ==  0) || (Context->OpenParams.Port == NULL))
	{
		return HAL_ERROR;
	}

	HAL_GPIO_WritePin(Context->OpenParams.Port, Context->OpenParams.Pin, state);
	return HAL_OK;
}

HAL_StatusTypeDef DrvGpio_Toggle(uint16_t gpioIndex)
{
	DrvGpio_Context_t* Context = DrvGpio_GetContext(gpioIndex);

	if ((Context->IsOpen ==  0) || (Context->OpenParams.Port == NULL))
	{
		return HAL_ERROR;
	}

	HAL_GPIO_TogglePin(Context->OpenParams.Port, Context->OpenParams.Pin);
	return HAL_OK;
}

HAL_StatusTypeDef DrvGpio_Ioctl(DrvGpio_IoctlCommand_t command, void* vpParam)
{
	switch (command)
	{
	case DRV_GPIO_IOCTL_INIT_INTERRUPT:
	{
		DrvGpio_InterruptInit_t * initParams = (DrvGpio_InterruptInit_t *) vpParam;
		DrvGpio_Context_t* context = NULL;

		if (initParams == NULL)
		{
			return HAL_ERROR;
		}

		context = DrvGpio_GetContext(initParams->GPIOIndex);
		if ((context == NULL) || (context->IsOpen == 0U))
		{
			return HAL_ERROR;
		}
		DrvGpio_InitInterrupt(initParams);

		return HAL_OK;
	}
	case DRV_GPIO_IOCTL_ATTACH_INTERRUPT:
	{
		DrvGpio_AttachInterruptRequest_t * attachParams = (DrvGpio_AttachInterruptRequest_t *) vpParam;
		DrvGpio_Context_t* context = NULL;

		if (attachParams == NULL)
		{
			return HAL_ERROR;
		}

		context = DrvGpio_GetContext(attachParams->GPIOIndex);
		if ((context == NULL) || (context->IsOpen == 0U))
		{
			return HAL_ERROR;
		}

		context->Callback = attachParams->Callback;
		context->UserData = attachParams->UserData;
		return HAL_OK;
	}
	default:
		return HAL_ERROR;
	}
}

HAL_StatusTypeDef DrvGpio_IrqHandler(void)
{
	uint32_t activeException = __get_IPSR();
	IRQn_Type activeIrq = (IRQn_Type) ((int32_t) activeException - 16);
	uint32_t index = 0U;
	uint8_t handled = 0U;

	if (activeException < 16U)
	{
		return HAL_ERROR;
	}

	for (index = 0U; index < DRV_GPIO_MAX_INSTANCES; ++index)
	{
		DrvGpio_Context_t* context = &g_gpioContexts[index];

		if ((context->IsOpen == 0U) ||
		    (context->OpenParams.InitInterrupt.InterruptEnabled == 0U) ||
		    (DrvGpio_ModeUsesInterrupt(context->OpenParams.Mode) == 0U) ||
		    (context->OpenParams.InitInterrupt.InterruptIRQn != activeIrq))
		{
			continue;
		}

		if (__HAL_GPIO_EXTI_GET_IT(context->OpenParams.Pin) == RESET)
		{
			continue;
		}

		__HAL_GPIO_EXTI_CLEAR_IT(context->OpenParams.Pin);
		if (context->Callback != NULL)
		{
			context->Callback(context->OpenParams.GPIOIndex, context->UserData);
		}

		handled = 1U;
	}

	return (handled != 0U) ? HAL_OK : HAL_ERROR;
}
