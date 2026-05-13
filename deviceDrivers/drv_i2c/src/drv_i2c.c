#include "drv_i2c/inc/drv_i2c.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
	I2C_HandleTypeDef* handle;
	I2C_HandleTypeDef ownedHandle;
	DrvI2c_OpenParams_t openParams;
} DrvI2c_Context_t;

static DrvI2c_Context_t g_i2cContexts[DRV_I2C_MAX_INSTANCES];

static DrvI2c_Context_t* DrvI2c_GetContext(uint32_t index)
{
	if (index >= DRV_I2C_MAX_INSTANCES)
	{
		return NULL;
	}

	return &g_i2cContexts[index];
}

static HAL_StatusTypeDef DrvI2c_CloseInternal(uint32_t index);

static HAL_StatusTypeDef DrvI2c_ConfigClockSource(I2C_TypeDef* instance)
{
	RCC_PeriphCLKInitTypeDef periphClkInitStruct = {0};

	if (instance == I2C1)
	{
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
		periphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
		return HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);
	}
	if (instance == I2C2)
	{
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
		periphClkInitStruct.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
		return HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);
	}
	if (instance == I2C3)
	{
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C3;
		periphClkInitStruct.I2c3ClockSelection = RCC_I2C3CLKSOURCE_PCLK1;
		return HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);
	}
#ifdef I2C4
	if (instance == I2C4)
	{
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
		periphClkInitStruct.I2c4ClockSelection = RCC_I2C4CLKSOURCE_PCLK1;
		return HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);
	}
#endif

	return HAL_ERROR;
}

static HAL_StatusTypeDef DrvI2c_EnableClock(I2C_TypeDef* instance)
{
	if (instance == I2C1)
	{
		__HAL_RCC_I2C1_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == I2C2)
	{
		__HAL_RCC_I2C2_CLK_ENABLE();
		return HAL_OK;
	}
	if (instance == I2C3)
	{
		__HAL_RCC_I2C3_CLK_ENABLE();
		return HAL_OK;
	}
#ifdef I2C4
	if (instance == I2C4)
	{
		__HAL_RCC_I2C4_CLK_ENABLE();
		return HAL_OK;
	}
#endif

	return HAL_ERROR;
}

static void DrvI2c_DisableClock(I2C_TypeDef* instance)
{
	if (instance == I2C1)
	{
		__HAL_RCC_I2C1_CLK_DISABLE();
	}
	else if (instance == I2C2)
	{
		__HAL_RCC_I2C2_CLK_DISABLE();
	}
	else if (instance == I2C3)
	{
		__HAL_RCC_I2C3_CLK_DISABLE();
	}
#ifdef I2C4
	else if (instance == I2C4)
	{
		__HAL_RCC_I2C4_CLK_DISABLE();
	}
#endif
}

static HAL_StatusTypeDef DrvI2c_PrepareBusPin(const DrvGpio_OpenParams_t* source, DrvGpio_OpenParams_t* prepared)
{
	if ((source == NULL) || (prepared == NULL) || (source->Port == NULL) || (source->Pin == 0U))
	{
		return HAL_ERROR;
	}

	*prepared = *source;
	prepared->Mode = GPIO_MODE_AF_OD;
	prepared->Pull = GPIO_NOPULL;
	prepared->Speed = GPIO_SPEED_FREQ_VERY_HIGH;

	return HAL_OK;
}

static HAL_StatusTypeDef DrvI2c_InitPins(const DrvI2c_OpenParams_t* openParams)
{
	DrvGpio_OpenParams_t sdaPin = {0};
	DrvGpio_OpenParams_t sclPin = {0};

	if ((openParams == NULL) ||
	    (DrvI2c_PrepareBusPin(&openParams->GPIO_SDAInit, &sdaPin) != HAL_OK) ||
	    (DrvI2c_PrepareBusPin(&openParams->GPIO_SCLInit, &sclPin) != HAL_OK))
	{
		return HAL_ERROR;
	}

	if (DrvGpio_Open(&sdaPin) != HAL_OK)
	{
		return HAL_ERROR;
	}

	return DrvGpio_Open(&sclPin);
}

static HAL_StatusTypeDef DrvI2c_InitOwnedHandle(DrvI2c_Context_t* context,
                                                const DrvI2c_OpenParams_t* openParams)
{
	HAL_StatusTypeDef status = HAL_OK;

	if ((context == NULL) || (openParams == NULL) || (openParams->I2C_Init.Instance == NULL))
	{
		return HAL_ERROR;
	}

	memset(&context->ownedHandle, 0, sizeof(context->ownedHandle));
	context->ownedHandle.Instance = openParams->I2C_Init.Instance;
	context->ownedHandle.Init.Timing = openParams->I2C_Init.ClockSpeed;
	context->ownedHandle.Init.OwnAddress1 = 0U;
	context->ownedHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	context->ownedHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	context->ownedHandle.Init.OwnAddress2 = 0U;
	context->ownedHandle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	context->ownedHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	context->ownedHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

	context->handle = &context->ownedHandle;

	status = DrvI2c_ConfigClockSource(openParams->I2C_Init.Instance);
	if (status != HAL_OK)
	{
		return status;
	}

	status = DrvI2c_InitPins(openParams);
	if (status != HAL_OK)
	{
		return status;
	}

	status = DrvI2c_EnableClock(openParams->I2C_Init.Instance);
	if (status != HAL_OK)
	{
		return status;
	}

	status = HAL_I2C_Init(context->handle);
	if (status != HAL_OK)
	{
		return status;
	}

	status = HAL_I2CEx_ConfigAnalogFilter(context->handle, I2C_ANALOGFILTER_ENABLE);
	if (status != HAL_OK)
	{
		return status;
	}

	return HAL_I2CEx_ConfigDigitalFilter(context->handle, 0U);
}

HAL_StatusTypeDef DrvI2c_Open(void* vpParam)
{
	const DrvI2c_OpenParams_t* openParams = (const DrvI2c_OpenParams_t*) vpParam;
	DrvI2c_Context_t* context = NULL;

	if ((openParams == NULL) || (openParams->I2CIndex >= DRV_I2C_MAX_INSTANCES))
	{
		return HAL_ERROR;
	}

	context = DrvI2c_GetContext(openParams->I2CIndex);
	if (context == NULL)
	{
		return HAL_ERROR;
	}

	memset(context, 0, sizeof(*context));
	context->openParams = *openParams;

	if (openParams->I2CHandle != NULL)
	{
		context->handle = openParams->I2CHandle;
		return HAL_OK;
	}

	return DrvI2c_InitOwnedHandle(context, openParams);
}

static HAL_StatusTypeDef DrvI2c_CloseInternal(uint32_t index)
{
	DrvI2c_Context_t* context = DrvI2c_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	if (context->handle == &context->ownedHandle)
	{
		(void) HAL_I2C_DeInit(context->handle);
		(void) DrvGpio_Close(&context->openParams.GPIO_SDAInit);
		(void) DrvGpio_Close(&context->openParams.GPIO_SCLInit);
		DrvI2c_DisableClock(context->handle->Instance);
	}

	memset(context, 0, sizeof(*context));
	return HAL_OK;
}

HAL_StatusTypeDef DrvI2c_Close(void* vpParam)
{
	const DrvI2c_DeviceParams_t* deviceParams = (const DrvI2c_DeviceParams_t*) vpParam;

	if (deviceParams == NULL)
	{
		return HAL_ERROR;
	}

	return DrvI2c_CloseInternal(deviceParams->I2CIndex);
}

I2C_HandleTypeDef* DrvI2c_GetHandle(uint32_t index)
{
	DrvI2c_Context_t* context = DrvI2c_GetContext(index);

	if (context == NULL)
	{
		return NULL;
	}

	return context->handle;
}

HAL_StatusTypeDef DrvI2c_MemRead(void* vpParam, uint8_t* data, uint16_t length)
{
	const DrvI2c_MemTransferParams_t* transferParams = (const DrvI2c_MemTransferParams_t*) vpParam;
	I2C_HandleTypeDef* handle = NULL;

	if ((transferParams == NULL) || (data == NULL) || (length == 0U))
	{
		return HAL_ERROR;
	}

	handle = DrvI2c_GetHandle(transferParams->I2CIndex);
	if (handle == NULL)
	{
		return HAL_ERROR;
	}

	return HAL_I2C_Mem_Read(handle,
	                        transferParams->DevAddress,
	                        transferParams->MemAddress,
	                        transferParams->MemAddressSize,
	                        data,
	                        length,
	                        transferParams->Timeout);
}

HAL_StatusTypeDef DrvI2c_MemWrite(void* vpParam, const uint8_t* data, uint16_t length)
{
	const DrvI2c_MemTransferParams_t* transferParams = (const DrvI2c_MemTransferParams_t*) vpParam;
	I2C_HandleTypeDef* handle = NULL;

	if ((transferParams == NULL) || (data == NULL) || (length == 0U))
	{
		return HAL_ERROR;
	}

	handle = DrvI2c_GetHandle(transferParams->I2CIndex);
	if (handle == NULL)
	{
		return HAL_ERROR;
	}

	return HAL_I2C_Mem_Write(handle,
	                         transferParams->DevAddress,
	                         transferParams->MemAddress,
	                         transferParams->MemAddressSize,
	                         (uint8_t*) data,
	                         length,
	                         transferParams->Timeout);
}
