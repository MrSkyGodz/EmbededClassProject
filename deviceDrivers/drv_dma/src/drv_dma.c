#include "drv_dma/inc/drv_dma.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
	DMA_HandleTypeDef* handle;
	DMA_HandleTypeDef ownedHandle;
	DrvDma_OpenParams_t openParams;
	uint8_t ownsHandle;
	uint8_t interruptEnabled;
} DrvDma_Context_t;

typedef enum
{
	DRV_DMA_CONTROLLER_NONE = 0,
	DRV_DMA_CONTROLLER_DMA1,
	DRV_DMA_CONTROLLER_DMA2
} DrvDma_Controller_t;

static DrvDma_Context_t g_dmaContexts[DRV_DMA_MAX_INSTANCES];

static HAL_StatusTypeDef DrvDma_InitInterrupt(const DrvDma_InterruptInit_t* interruptInit);
static HAL_StatusTypeDef DrvDma_CloseInternal(uint32_t index);
static DrvDma_Controller_t DrvDma_GetController(const DMA_Stream_TypeDef* instance);

static DrvDma_Context_t* DrvDma_GetContext(uint32_t index)
{
	if (index >= DRV_DMA_MAX_INSTANCES)
	{
		return NULL;
	}

	return &g_dmaContexts[index];
}

static DrvDma_Controller_t DrvDma_GetController(const DMA_Stream_TypeDef* instance)
{
	if ((instance == DMA1_Stream0) || (instance == DMA1_Stream1) ||
	    (instance == DMA1_Stream2) || (instance == DMA1_Stream3) ||
	    (instance == DMA1_Stream4) || (instance == DMA1_Stream5) ||
	    (instance == DMA1_Stream6) || (instance == DMA1_Stream7))
	{
		return DRV_DMA_CONTROLLER_DMA1;
	}

	if ((instance == DMA2_Stream0) || (instance == DMA2_Stream1) ||
	    (instance == DMA2_Stream2) || (instance == DMA2_Stream3) ||
	    (instance == DMA2_Stream4) || (instance == DMA2_Stream5) ||
	    (instance == DMA2_Stream6) || (instance == DMA2_Stream7))
	{
		return DRV_DMA_CONTROLLER_DMA2;
	}

	return DRV_DMA_CONTROLLER_NONE;
}

HAL_StatusTypeDef DrvDma_EnableInterrupt(uint32_t index)
{
	DrvDma_Context_t* context = DrvDma_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	if (context->interruptEnabled != 0U)
	{
		return HAL_OK;
	}

	if (DrvDma_InitInterrupt(&context->openParams.Interrupt) != HAL_OK)
	{
		return HAL_ERROR;
	}

	context->interruptEnabled = 1U;
	return HAL_OK;
}

HAL_StatusTypeDef DrvDma_DisableInterrupt(uint32_t index)
{
	DrvDma_Context_t* context = DrvDma_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	if ((context->openParams.Interrupt.Enabled != 0U) &&
	    (context->interruptEnabled != 0U))
	{
		HAL_NVIC_DisableIRQ(context->openParams.Interrupt.IRQn);
		context->interruptEnabled = 0U;
	}

	return HAL_OK;
}

static HAL_StatusTypeDef DrvDma_EnableClock(DMA_Stream_TypeDef* instance)
{
	switch (DrvDma_GetController(instance))
	{
	case DRV_DMA_CONTROLLER_DMA1:
		__HAL_RCC_DMA1_CLK_ENABLE();
		return HAL_OK;

	case DRV_DMA_CONTROLLER_DMA2:
		__HAL_RCC_DMA2_CLK_ENABLE();
		return HAL_OK;

	case DRV_DMA_CONTROLLER_NONE:
	default:
		return HAL_ERROR;
	}
}

static void DrvDma_DisableClock(DMA_Stream_TypeDef* instance)
{
	switch (DrvDma_GetController(instance))
	{
	case DRV_DMA_CONTROLLER_DMA1:
		__HAL_RCC_DMA1_CLK_DISABLE();
		return;

	case DRV_DMA_CONTROLLER_DMA2:
		__HAL_RCC_DMA2_CLK_DISABLE();
		return;

	case DRV_DMA_CONTROLLER_NONE:
	default:
		return;
	}
}

static HAL_StatusTypeDef DrvDma_InitInterrupt(const DrvDma_InterruptInit_t* interruptInit)
{
	if ((interruptInit == NULL) || (interruptInit->Enabled == 0U))
	{
		return HAL_OK;
	}

	HAL_NVIC_SetPriority(interruptInit->IRQn,
	                     interruptInit->Priority,
	                     interruptInit->SubPriority);
	HAL_NVIC_EnableIRQ(interruptInit->IRQn);
	return HAL_OK;
}

static HAL_StatusTypeDef DrvDma_InitOwnedHandle(DrvDma_Context_t* context, const DrvDma_OpenParams_t* openParams)
{
	if ((context == NULL) || (openParams == NULL) || (openParams->DMA_Init.Instance == NULL))
	{
		return HAL_ERROR;
	}

	memset(&context->ownedHandle, 0, sizeof(context->ownedHandle));
	context->ownedHandle.Instance = openParams->DMA_Init.Instance;
	context->ownedHandle.Init.Channel = openParams->DMA_Init.Channel;
	context->ownedHandle.Init.Direction = openParams->DMA_Init.Direction;
	context->ownedHandle.Init.PeriphInc = openParams->DMA_Init.PeriphInc;
	context->ownedHandle.Init.MemInc = openParams->DMA_Init.MemInc;
	context->ownedHandle.Init.PeriphDataAlignment = openParams->DMA_Init.PeriphDataAlignment;
	context->ownedHandle.Init.MemDataAlignment = openParams->DMA_Init.MemDataAlignment;
	context->ownedHandle.Init.Mode = openParams->DMA_Init.Mode;
	context->ownedHandle.Init.Priority = openParams->DMA_Init.Priority;
	context->ownedHandle.Init.FIFOMode = openParams->DMA_Init.FIFOMode;
	context->ownedHandle.Init.FIFOThreshold = openParams->DMA_Init.FIFOThreshold;
	context->ownedHandle.Init.MemBurst = openParams->DMA_Init.MemBurst;
	context->ownedHandle.Init.PeriphBurst = openParams->DMA_Init.PeriphBurst;

	context->handle = &context->ownedHandle;
	context->ownsHandle = 1U;

	if (DrvDma_EnableClock(openParams->DMA_Init.Instance) != HAL_OK)
	{
		return HAL_ERROR;
	}

	return HAL_DMA_Init(context->handle);
}

HAL_StatusTypeDef DrvDma_Open(void* vpParam)
{
	const DrvDma_OpenParams_t* openParams = (const DrvDma_OpenParams_t*) vpParam;
	DrvDma_Context_t* context = NULL;

	if ((openParams == NULL) || (openParams->DMAIndex >= DRV_DMA_MAX_INSTANCES))
	{
		return HAL_ERROR;
	}

	context = DrvDma_GetContext(openParams->DMAIndex);
	if (context == NULL)
	{
		return HAL_ERROR;
	}

	memset(context, 0, sizeof(*context));
	context->openParams = *openParams;

	if (openParams->DMAHandle != NULL)
	{
		context->handle = openParams->DMAHandle;
		return HAL_OK;
	}

	return DrvDma_InitOwnedHandle(context, openParams);
}

static HAL_StatusTypeDef DrvDma_CloseInternal(uint32_t index)
{
	DrvDma_Context_t* context = DrvDma_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	if (context->ownsHandle != 0U)
	{
		(void) HAL_DMA_DeInit(context->handle);
		DrvDma_DisableClock(context->openParams.DMA_Init.Instance);
	}

	(void) DrvDma_DisableInterrupt(index);

	memset(context, 0, sizeof(*context));
	return HAL_OK;
}

HAL_StatusTypeDef DrvDma_Close(void* vpParam)
{
	const DrvDma_DeviceParams_t* deviceParams = (const DrvDma_DeviceParams_t*) vpParam;

	if (deviceParams == NULL)
	{
		return HAL_ERROR;
	}

	return DrvDma_CloseInternal(deviceParams->DMAIndex);
}

DMA_HandleTypeDef* DrvDma_GetHandle(uint32_t index)
{
	DrvDma_Context_t* context = DrvDma_GetContext(index);

	if (context == NULL)
	{
		return NULL;
	}

	return context->handle;
}

HAL_StatusTypeDef DrvDma_Start(uint32_t index,
                               uint32_t srcAddress,
                               uint32_t dstAddress,
                               uint32_t dataLength)
{
	DMA_HandleTypeDef* handle = DrvDma_GetHandle(index);

	if (handle == NULL)
	{
		return HAL_ERROR;
	}

	return HAL_DMA_Start(handle, srcAddress, dstAddress, dataLength);
}

HAL_StatusTypeDef DrvDma_StartIT(uint32_t index,
                                 uint32_t srcAddress,
                                 uint32_t dstAddress,
                                 uint32_t dataLength)
{
	DMA_HandleTypeDef* handle = DrvDma_GetHandle(index);

	if (handle == NULL)
	{
		return HAL_ERROR;
	}

	if (DrvDma_EnableInterrupt(index) != HAL_OK)
	{
		return HAL_ERROR;
	}

	return HAL_DMA_Start_IT(handle, srcAddress, dstAddress, dataLength);
}

HAL_StatusTypeDef DrvDma_Abort(uint32_t index)
{
	DMA_HandleTypeDef* handle = DrvDma_GetHandle(index);

	if (handle == NULL)
	{
		return HAL_ERROR;
	}

	return HAL_DMA_Abort(handle);
}

HAL_StatusTypeDef DrvDma_IrqHandler(void)
{
	IRQn_Type activeIrq = NonMaskableInt_IRQn;
	uint32_t exceptionNumber = __get_IPSR() & 0x1FFU;
	uint32_t index = 0U;

	if (exceptionNumber < 16U)
	{
		return HAL_ERROR;
	}

	activeIrq = (IRQn_Type) ((int32_t) exceptionNumber - 16);

	for (index = 0U; index < DRV_DMA_MAX_INSTANCES; ++index)
	{
		DrvDma_Context_t* context = &g_dmaContexts[index];

		if ((context->handle == NULL) ||
		    (context->openParams.Interrupt.Enabled == 0U) ||
		    (context->openParams.Interrupt.IRQn != activeIrq))
		{
			continue;
		}

		HAL_DMA_IRQHandler(context->handle);
		if (context->openParams.Callback != NULL)
		{
			context->openParams.Callback(context->openParams.CallbackUserData);
		}
		return HAL_OK;
	}

	return HAL_ERROR;
}
