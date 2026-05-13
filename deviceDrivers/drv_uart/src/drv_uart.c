#include "drv_uart/inc/drv_uart.h"

#include <stddef.h>
#include <string.h>

#include "drv_dma/inc/drv_dma.h"

typedef struct
{
	UART_HandleTypeDef* handle;
	UART_HandleTypeDef ownedHandle;
	DrvUart_OpenParams_t openParams;
	DrvDma_OpenParams_t rxDmaOpenParams;
	DrvDma_OpenParams_t txDmaOpenParams;
	DrvUart_Callback_t callback;
	void* callbackUserData;
	uint8_t* rxBuffer;
	size_t rxBufferSize;
	size_t rxReportedIndex;
} DrvUart_Context_t;

typedef enum
{
	DRV_UART_INSTANCE_NONE = 0,
	DRV_UART_INSTANCE_USART1,
	DRV_UART_INSTANCE_USART2,
	DRV_UART_INSTANCE_USART3,
	DRV_UART_INSTANCE_USART6,
#ifdef UART4
	DRV_UART_INSTANCE_UART4,
#endif
#ifdef UART5
	DRV_UART_INSTANCE_UART5,
#endif
#ifdef UART7
	DRV_UART_INSTANCE_UART7,
#endif
#ifdef UART8
	DRV_UART_INSTANCE_UART8,
#endif
} DrvUart_InstanceId_t;

static DrvUart_Context_t g_uartContexts[DRV_UART_MAX_INSTANCES];

static DrvUart_Context_t* DrvUart_GetContext(uint32_t index);
static HAL_StatusTypeDef DrvUart_CloseInternal(uint32_t index);
static HAL_StatusTypeDef DrvUart_AbortInternal(uint32_t index);
static HAL_StatusTypeDef DrvUart_AttachInterruptInternal(const DrvUart_AttachInterruptRequest_t* request);
static DrvUart_InstanceId_t DrvUart_GetInstanceId(const USART_TypeDef* instance);
static void DrvUart_ClearReadState(DrvUart_Context_t* context);
static size_t DrvUart_GetReceivedLength(const DrvUart_Context_t* context);
static DrvUart_Event_t DrvUart_MapRxEvent(const DrvUart_Context_t* context,
	                                      HAL_UART_RxEventTypeTypeDef halEvent);
static void DrvUart_EmitRxChunk(const DrvUart_Context_t* context,
	                            DrvUart_Event_t uartEvent,
	                            size_t chunkOffset,
	                            size_t chunkLength);
static void DrvUart_ProcessRxEvent(DrvUart_Context_t* context);
static void DrvUart_OnRxDmaInterrupt(void* userData);
static HAL_StatusTypeDef DrvUart_ConfigClockSource(USART_TypeDef* instance);
static HAL_StatusTypeDef DrvUart_EnableClock(USART_TypeDef* instance);
static void DrvUart_DisableClock(USART_TypeDef* instance);
static HAL_StatusTypeDef DrvUart_PreparePin(const DrvGpio_OpenParams_t* source, DrvGpio_OpenParams_t* prepared);
static HAL_StatusTypeDef DrvUart_InitPins(const DrvUart_OpenParams_t* openParams);
static HAL_StatusTypeDef DrvUart_InitInterrupt(const DrvUart_InterruptInit_t* interruptInit);
static HAL_StatusTypeDef DrvUart_EnableInterrupts(DrvUart_Context_t* context,
	                                              DrvUart_Mode_t mode,
	                                              DMA_HandleTypeDef* dmaHandle,
	                                              uint32_t dmaIndex);
static HAL_StatusTypeDef DrvUart_InitOwnedHandle(DrvUart_Context_t* context,
	                                             const DrvUart_OpenParams_t* openParams);
static HAL_StatusTypeDef DrvUart_BuildDmaOpenParams(const DrvUart_Context_t* context,
	                                                uint8_t isRx,
	                                                DrvDma_OpenParams_t* openParams);
static HAL_StatusTypeDef DrvUart_LinkDma(DrvUart_Context_t* context, uint8_t isRx);

static DrvUart_Context_t* DrvUart_GetContext(uint32_t index)
{
	if (index >= DRV_UART_MAX_INSTANCES)
	{
		return NULL;
	}

	return &g_uartContexts[index];
}

static DrvUart_InstanceId_t DrvUart_GetInstanceId(const USART_TypeDef* instance)
{
	if (instance == USART1)
	{
		return DRV_UART_INSTANCE_USART1;
	}

	if (instance == USART2)
	{
		return DRV_UART_INSTANCE_USART2;
	}

	if (instance == USART3)
	{
		return DRV_UART_INSTANCE_USART3;
	}

	if (instance == USART6)
	{
		return DRV_UART_INSTANCE_USART6;
	}

#ifdef UART4
	if (instance == UART4)
	{
		return DRV_UART_INSTANCE_UART4;
	}
#endif
#ifdef UART5
	if (instance == UART5)
	{
		return DRV_UART_INSTANCE_UART5;
	}
#endif
#ifdef UART7
	if (instance == UART7)
	{
		return DRV_UART_INSTANCE_UART7;
	}
#endif
#ifdef UART8
	if (instance == UART8)
	{
		return DRV_UART_INSTANCE_UART8;
	}
#endif

	return DRV_UART_INSTANCE_NONE;
}

static void DrvUart_ClearReadState(DrvUart_Context_t* context)
{
	if (context == NULL)
	{
		return;
	}

	context->rxBuffer = NULL;
	context->rxBufferSize = 0U;
	context->rxReportedIndex = 0U;
}

static DrvUart_Event_t DrvUart_MapRxEvent(const DrvUart_Context_t* context,
	                                      HAL_UART_RxEventTypeTypeDef halEvent)
{
	if (halEvent == HAL_UART_RXEVENT_IDLE)
	{
		return DRV_UART_EVENT_IDLE;
	}

	if (halEvent == HAL_UART_RXEVENT_HT)
	{
		return DRV_UART_EVENT_DATA;
	}

	if ((context != NULL) &&
	    (context->openParams.RxMode == DRV_UART_MODE_INTERRUPT) &&
	    (context->rxBufferSize == 1U))
	{
		return DRV_UART_EVENT_DATA;
	}

	return DRV_UART_EVENT_BUFFER_FULL;
}

static void DrvUart_EmitRxChunk(const DrvUart_Context_t* context,
	                            DrvUart_Event_t uartEvent,
	                            size_t chunkOffset,
	                            size_t chunkLength)
{
	if ((context == NULL) ||
	    (context->callback == NULL) ||
	    (context->rxBuffer == NULL) ||
	    (chunkLength == 0U))
	{
		return;
	}

	context->callback(uartEvent,
	                  context->rxBuffer + chunkOffset,
	                  chunkLength,
	                  context->callbackUserData);
}

static void DrvUart_ProcessRxEvent(DrvUart_Context_t* context)
{
	HAL_UART_RxEventTypeTypeDef halEvent = HAL_UART_RXEVENT_TC;
	DrvUart_Event_t uartEvent = DRV_UART_EVENT_BUFFER_FULL;
	size_t writeIndex = 0U;
	size_t chunkLength = 0U;
	size_t chunkOffset = 0U;

	if ((context == NULL) ||
	    (context->handle == NULL) ||
	    (context->rxBuffer == NULL) ||
	    (context->rxBufferSize == 0U))
	{
		return;
	}

	halEvent = HAL_UARTEx_GetRxEventType(context->handle);
	writeIndex = DrvUart_GetReceivedLength(context);
	if (writeIndex > context->rxBufferSize)
	{
		return;
	}

	uartEvent = DrvUart_MapRxEvent(context, halEvent);
	if ((context->callback == NULL) || (writeIndex == context->rxReportedIndex))
	{
		context->rxReportedIndex = writeIndex;
		return;
	}

	if (writeIndex > context->rxReportedIndex)
	{
		chunkOffset = context->rxReportedIndex;
		chunkLength = writeIndex - context->rxReportedIndex;
		DrvUart_EmitRxChunk(context, uartEvent, chunkOffset, chunkLength);
		context->rxReportedIndex = writeIndex;
		return;
	}

	chunkOffset = context->rxReportedIndex;
	chunkLength = context->rxBufferSize - context->rxReportedIndex;
	DrvUart_EmitRxChunk(context, uartEvent, chunkOffset, chunkLength);

	context->rxReportedIndex = 0U;
	DrvUart_EmitRxChunk(context, uartEvent, 0U, writeIndex);

	context->rxReportedIndex = writeIndex;
}

static void DrvUart_OnRxDmaInterrupt(void* userData)
{
	DrvUart_ProcessRxEvent((DrvUart_Context_t*) userData);
}

static HAL_StatusTypeDef DrvUart_ConfigClockSource(USART_TypeDef* instance)
{
	RCC_PeriphCLKInitTypeDef periphClkInitStruct = {0};

	switch (DrvUart_GetInstanceId(instance))
	{
	case DRV_UART_INSTANCE_USART1:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
		periphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
		break;

	case DRV_UART_INSTANCE_USART2:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2;
		periphClkInitStruct.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
		break;

	case DRV_UART_INSTANCE_USART3:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3;
		periphClkInitStruct.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
		break;

	case DRV_UART_INSTANCE_USART6:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART6;
		periphClkInitStruct.Usart6ClockSelection = RCC_USART6CLKSOURCE_PCLK2;
		break;

#ifdef UART4
	case DRV_UART_INSTANCE_UART4:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_UART4;
		periphClkInitStruct.Uart4ClockSelection = RCC_UART4CLKSOURCE_PCLK1;
		break;
#endif
#ifdef UART5
	case DRV_UART_INSTANCE_UART5:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_UART5;
		periphClkInitStruct.Uart5ClockSelection = RCC_UART5CLKSOURCE_PCLK1;
		break;
#endif
#ifdef UART7
	case DRV_UART_INSTANCE_UART7:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_UART7;
		periphClkInitStruct.Uart7ClockSelection = RCC_UART7CLKSOURCE_PCLK1;
		break;
#endif
#ifdef UART8
	case DRV_UART_INSTANCE_UART8:
		periphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_UART8;
		periphClkInitStruct.Uart8ClockSelection = RCC_UART8CLKSOURCE_PCLK1;
		break;
#endif
	case DRV_UART_INSTANCE_NONE:
	default:
		return HAL_ERROR;
	}

	return HAL_RCCEx_PeriphCLKConfig(&periphClkInitStruct);
}

static HAL_StatusTypeDef DrvUart_EnableClock(USART_TypeDef* instance)
{
	switch (DrvUart_GetInstanceId(instance))
	{
	case DRV_UART_INSTANCE_USART1:
		__HAL_RCC_USART1_CLK_ENABLE();
		return HAL_OK;

	case DRV_UART_INSTANCE_USART2:
		__HAL_RCC_USART2_CLK_ENABLE();
		return HAL_OK;

	case DRV_UART_INSTANCE_USART3:
		__HAL_RCC_USART3_CLK_ENABLE();
		return HAL_OK;

	case DRV_UART_INSTANCE_USART6:
		__HAL_RCC_USART6_CLK_ENABLE();
		return HAL_OK;

#ifdef UART4
	case DRV_UART_INSTANCE_UART4:
		__HAL_RCC_UART4_CLK_ENABLE();
		return HAL_OK;
#endif

#ifdef UART5
	case DRV_UART_INSTANCE_UART5:
		__HAL_RCC_UART5_CLK_ENABLE();
		return HAL_OK;
#endif

#ifdef UART7
	case DRV_UART_INSTANCE_UART7:
		__HAL_RCC_UART7_CLK_ENABLE();
		return HAL_OK;
#endif

#ifdef UART8
	case DRV_UART_INSTANCE_UART8:
		__HAL_RCC_UART8_CLK_ENABLE();
		return HAL_OK;
#endif

	case DRV_UART_INSTANCE_NONE:
	default:
		return HAL_ERROR;
	}
}

static void DrvUart_DisableClock(USART_TypeDef* instance)
{
	switch (DrvUart_GetInstanceId(instance))
	{
	case DRV_UART_INSTANCE_USART1:
		__HAL_RCC_USART1_CLK_DISABLE();
		return;

	case DRV_UART_INSTANCE_USART2:
		__HAL_RCC_USART2_CLK_DISABLE();
		return;

	case DRV_UART_INSTANCE_USART3:
		__HAL_RCC_USART3_CLK_DISABLE();
		return;

	case DRV_UART_INSTANCE_USART6:
		__HAL_RCC_USART6_CLK_DISABLE();
		return;

#ifdef UART4
	case DRV_UART_INSTANCE_UART4:
		__HAL_RCC_UART4_CLK_DISABLE();
		return;
#endif
#ifdef UART5
	case DRV_UART_INSTANCE_UART5:
		__HAL_RCC_UART5_CLK_DISABLE();
		return;
#endif
#ifdef UART7
	case DRV_UART_INSTANCE_UART7:
		__HAL_RCC_UART7_CLK_DISABLE();
		return;
#endif
#ifdef UART8
	case DRV_UART_INSTANCE_UART8:
		__HAL_RCC_UART8_CLK_DISABLE();
		return;
#endif

	case DRV_UART_INSTANCE_NONE:
	default:
		return;
	}
}

static HAL_StatusTypeDef DrvUart_PreparePin(const DrvGpio_OpenParams_t* source, DrvGpio_OpenParams_t* prepared)
{
	if ((source == NULL) || (prepared == NULL) || (source->Port == NULL) || (source->Pin == 0U))
	{
		return HAL_ERROR;
	}

	*prepared = *source;
	prepared->Mode = GPIO_MODE_AF_PP;
	prepared->Pull = GPIO_NOPULL;
	prepared->Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	return HAL_OK;
}

static HAL_StatusTypeDef DrvUart_InitPins(const DrvUart_OpenParams_t* openParams)
{
	DrvGpio_OpenParams_t txPin = {0};
	DrvGpio_OpenParams_t rxPin = {0};

	if ((openParams == NULL) ||
	    (DrvUart_PreparePin(&openParams->GPIO_TXInit, &txPin) != HAL_OK) ||
	    (DrvUart_PreparePin(&openParams->GPIO_RXInit, &rxPin) != HAL_OK))
	{
		return HAL_ERROR;
	}

	if (DrvGpio_Open(&txPin) != HAL_OK)
	{
		return HAL_ERROR;
	}

	return DrvGpio_Open(&rxPin);
}

static HAL_StatusTypeDef DrvUart_InitInterrupt(const DrvUart_InterruptInit_t* interruptInit)
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

static HAL_StatusTypeDef DrvUart_EnableInterrupts(DrvUart_Context_t* context,
	                                              DrvUart_Mode_t mode,
	                                              DMA_HandleTypeDef* dmaHandle,
	                                              uint32_t dmaIndex)
{
	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	if (mode == DRV_UART_MODE_BLOCKING)
	{
		return HAL_OK;
	}

	if (context->openParams.Interrupt.Enabled == 0U)
	{
		return HAL_ERROR;
	}

	if (DrvUart_InitInterrupt(&context->openParams.Interrupt) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (mode != DRV_UART_MODE_DMA)
	{
		return HAL_OK;
	}

	if (dmaHandle == NULL)
	{
		return HAL_ERROR;
	}

	return DrvDma_EnableInterrupt(dmaIndex);
}

static HAL_StatusTypeDef DrvUart_InitOwnedHandle(DrvUart_Context_t* context,
	                                             const DrvUart_OpenParams_t* openParams)
{
	if ((context == NULL) || (openParams == NULL) || (openParams->UART_Init.Instance == NULL))
	{
		return HAL_ERROR;
	}

	memset(&context->ownedHandle, 0, sizeof(context->ownedHandle));
	context->ownedHandle.Instance = openParams->UART_Init.Instance;
	context->ownedHandle.Init.BaudRate = openParams->UART_Init.BaudRate;
	context->ownedHandle.Init.WordLength = openParams->UART_Init.WordLength;
	context->ownedHandle.Init.StopBits = openParams->UART_Init.StopBits;
	context->ownedHandle.Init.Parity = openParams->UART_Init.Parity;
	context->ownedHandle.Init.Mode = openParams->UART_Init.Mode;
	context->ownedHandle.Init.HwFlowCtl = openParams->UART_Init.HwFlowCtl;
	context->ownedHandle.Init.OverSampling = openParams->UART_Init.OverSampling;
	context->ownedHandle.Init.OneBitSampling = openParams->UART_Init.OneBitSampling;
	context->ownedHandle.AdvancedInit.AdvFeatureInit = openParams->UART_Init.AdvancedFeatureInit;

	context->handle = &context->ownedHandle;

	if (DrvUart_ConfigClockSource(openParams->UART_Init.Instance) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (DrvUart_InitPins(openParams) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (DrvUart_EnableClock(openParams->UART_Init.Instance) != HAL_OK)
	{
		return HAL_ERROR;
	}

	return HAL_UART_Init(context->handle);
}

static HAL_StatusTypeDef DrvUart_BuildDmaOpenParams(const DrvUart_Context_t* context,
	                                                uint8_t isRx,
	                                                DrvDma_OpenParams_t* openParams)
{
	const DrvUart_DmaConfig_t* dmaConfig = NULL;
	uint32_t priority = 5U;
	uint32_t subPriority = 0U;

	if ((context == NULL) || (openParams == NULL))
	{
		return HAL_ERROR;
	}

	dmaConfig = isRx ? &context->openParams.RxDmaConfig : &context->openParams.TxDmaConfig;

	if (context->openParams.Interrupt.Enabled != 0U)
	{
		priority = context->openParams.Interrupt.Priority;
		subPriority = context->openParams.Interrupt.SubPriority;
	}

	memset(openParams, 0, sizeof(*openParams));
	openParams->DMAIndex = dmaConfig->DmaIndex;

	if ((openParams->DMAIndex >= DRV_DMA_MAX_INSTANCES) ||
	    (dmaConfig->Instance == NULL) ||
	    (dmaConfig->IRQn == NonMaskableInt_IRQn))
	{
		return HAL_ERROR;
	}

	openParams->DMA_Init.Instance = dmaConfig->Instance;
	openParams->DMA_Init.Channel = dmaConfig->Channel;
	openParams->DMA_Init.Direction = isRx ? DMA_PERIPH_TO_MEMORY : DMA_MEMORY_TO_PERIPH;
	openParams->DMA_Init.PeriphInc = DMA_PINC_DISABLE;
	openParams->DMA_Init.MemInc = DMA_MINC_ENABLE;
	openParams->DMA_Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	openParams->DMA_Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	openParams->DMA_Init.Mode = isRx ? DMA_CIRCULAR : DMA_NORMAL;
	openParams->DMA_Init.Priority = DMA_PRIORITY_LOW;
	openParams->DMA_Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	openParams->DMA_Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
	openParams->DMA_Init.MemBurst = DMA_MBURST_SINGLE;
	openParams->DMA_Init.PeriphBurst = DMA_PBURST_SINGLE;
	openParams->Interrupt.Enabled = 1U;
	openParams->Interrupt.IRQn = dmaConfig->IRQn;
	openParams->Interrupt.Priority = priority;
	openParams->Interrupt.SubPriority = subPriority;
	openParams->DMAHandle = NULL;
	openParams->Callback = isRx ? DrvUart_OnRxDmaInterrupt : NULL;
	openParams->CallbackUserData = isRx ? (void*) context : NULL;
	return HAL_OK;
}

static HAL_StatusTypeDef DrvUart_LinkDma(DrvUart_Context_t* context, uint8_t isRx)
{
	DrvUart_Mode_t mode = DRV_UART_MODE_BLOCKING;
	DrvDma_OpenParams_t* dmaOpenParams = NULL;
	DMA_HandleTypeDef** dmaHandleSlot = NULL;
	DMA_HandleTypeDef* dmaHandle = NULL;

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	mode = isRx ? context->openParams.RxMode : context->openParams.TxMode;
	if (mode != DRV_UART_MODE_DMA)
	{
		return HAL_OK;
	}

	dmaOpenParams = isRx ? &context->rxDmaOpenParams : &context->txDmaOpenParams;
	dmaHandleSlot = isRx ? &context->handle->hdmarx : &context->handle->hdmatx;

	if (DrvUart_BuildDmaOpenParams(context, isRx, dmaOpenParams) != HAL_OK)
	{
		return HAL_ERROR;
	}

	if (DrvDma_Open(dmaOpenParams) != HAL_OK)
	{
		return HAL_ERROR;
	}

	dmaHandle = DrvDma_GetHandle(dmaOpenParams->DMAIndex);
	if (dmaHandle == NULL)
	{
		return HAL_ERROR;
	}

	*dmaHandleSlot = dmaHandle;
	dmaHandle->Parent = context->handle;
	return HAL_OK;
}

static size_t DrvUart_GetReceivedLength(const DrvUart_Context_t* context)
{
	size_t totalLength = 0U;

	if ((context == NULL) ||
	    (context->handle == NULL) ||
	    (context->rxBuffer == NULL) ||
	    (context->rxBufferSize == 0U))
	{
		return 0U;
	}

	if ((context->openParams.RxMode == DRV_UART_MODE_DMA) && (context->handle->hdmarx != NULL))
	{
		totalLength = context->rxBufferSize - (size_t) __HAL_DMA_GET_COUNTER(context->handle->hdmarx);
	}
	else
	{
		totalLength = context->rxBufferSize - (size_t) context->handle->RxXferCount;
	}

	if (totalLength > context->rxBufferSize)
	{
		return 0U;
	}

	return totalLength;
}

static HAL_StatusTypeDef DrvUart_AttachInterruptInternal(const DrvUart_AttachInterruptRequest_t* request)
{
	DrvUart_Context_t* context = NULL;
	HAL_StatusTypeDef status = HAL_OK;

	if ((request == NULL) ||
	    (request->UartIndex >= DRV_UART_MAX_INSTANCES) ||
	    (request->Buffer == NULL) ||
	    (request->BufferLength == 0U) ||
	    (request->BufferLength > 0xFFFFU))
	{
		return HAL_ERROR;
	}

	context = DrvUart_GetContext(request->UartIndex);
	if ((context == NULL) ||
	    (context->handle == NULL) ||
	    (context->openParams.RxMode == DRV_UART_MODE_BLOCKING))
	{
		return HAL_ERROR;
	}

	if (context->handle->RxState != HAL_UART_STATE_READY)
	{
		status = HAL_UART_AbortReceive(context->handle);
		if (status != HAL_OK)
		{
			return status;
		}
	}

	status = DrvUart_EnableInterrupts(context,
	                                  context->openParams.RxMode,
	                                  context->handle->hdmarx,
	                                  context->rxDmaOpenParams.DMAIndex);
	if (status != HAL_OK)
	{
		return status;
	}

	context->rxBuffer = (uint8_t*) request->Buffer;
	context->rxBufferSize = request->BufferLength;
	context->rxReportedIndex = 0U;

	if (context->openParams.RxMode == DRV_UART_MODE_DMA)
	{
		status = HAL_UARTEx_ReceiveToIdle_DMA(context->handle,
		                                      context->rxBuffer,
		                                      (uint16_t) context->rxBufferSize);
	}
	else
	{
		status = HAL_UARTEx_ReceiveToIdle_IT(context->handle,
		                                     context->rxBuffer,
		                                     (uint16_t) context->rxBufferSize);
	}

	if (status != HAL_OK)
	{
		DrvUart_ClearReadState(context);
	}

	return status;
}

HAL_StatusTypeDef DrvUart_Open(void* vpParam)
{
	const DrvUart_OpenParams_t* openParams = (const DrvUart_OpenParams_t*) vpParam;
	DrvUart_Context_t* context = NULL;
	HAL_StatusTypeDef status = HAL_OK;

	if ((openParams == NULL) || (openParams->UartIndex >= DRV_UART_MAX_INSTANCES))
	{
		return HAL_ERROR;
	}

	context = DrvUart_GetContext(openParams->UartIndex);
	if (context == NULL)
	{
		return HAL_ERROR;
	}

	memset(context, 0, sizeof(*context));
	context->openParams = *openParams;

	if (openParams->UARTHandle != NULL)
	{
		context->handle = openParams->UARTHandle;
	}
	else
	{
		status = DrvUart_InitOwnedHandle(context, openParams);
		if (status != HAL_OK)
		{
			return status;
		}
	}

	status = DrvUart_LinkDma(context, 1U);
	if (status != HAL_OK)
	{
		(void) DrvUart_CloseInternal(openParams->UartIndex);
		return status;
	}

	status = DrvUart_LinkDma(context, 0U);
	if (status != HAL_OK)
	{
		(void) DrvUart_CloseInternal(openParams->UartIndex);
		return status;
	}

	return HAL_OK;
}

static HAL_StatusTypeDef DrvUart_CloseInternal(uint32_t index)
{
	DrvUart_Context_t* context = DrvUart_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	if (context->handle->hdmarx != NULL)
	{
		DrvDma_DeviceParams_t rxDmaDeviceParams = {.DMAIndex = context->rxDmaOpenParams.DMAIndex};

		(void) DrvDma_Close(&rxDmaDeviceParams);
	}

	if (context->handle->hdmatx != NULL)
	{
		DrvDma_DeviceParams_t txDmaDeviceParams = {.DMAIndex = context->txDmaOpenParams.DMAIndex};

		(void) DrvDma_Close(&txDmaDeviceParams);
	}

	if (context->handle == &context->ownedHandle)
	{
		(void) HAL_UART_DeInit(context->handle);
		(void) DrvGpio_Close(&context->openParams.GPIO_TXInit);
		(void) DrvGpio_Close(&context->openParams.GPIO_RXInit);
		DrvUart_DisableClock(context->handle->Instance);
	}

	if (context->openParams.Interrupt.Enabled != 0U)
	{
		HAL_NVIC_DisableIRQ(context->openParams.Interrupt.IRQn);
	}

	DrvUart_ClearReadState(context);
	memset(context, 0, sizeof(*context));
	return HAL_OK;
}

HAL_StatusTypeDef DrvUart_Close(void* vpParam)
{
	const DrvUart_DeviceParams_t* deviceParams = (const DrvUart_DeviceParams_t*) vpParam;

	if (deviceParams == NULL)
	{
		return HAL_ERROR;
	}

	return DrvUart_CloseInternal(deviceParams->UartIndex);
}

HAL_StatusTypeDef DrvUart_Read(void* vpParam, void* pvBuffer, const uint32_t xBytes)
{
	const DrvUart_TransferParams_t* transferParams = (const DrvUart_TransferParams_t*) vpParam;
	DrvUart_Context_t* context = NULL;

	if ((transferParams == NULL) || (pvBuffer == NULL) || (xBytes == 0U) || (xBytes > 0xFFFFU))
	{
		return HAL_ERROR;
	}

	context = DrvUart_GetContext(transferParams->UartIndex);
	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	if (context->openParams.RxMode != DRV_UART_MODE_BLOCKING)
	{
		return HAL_ERROR;
	}

	return HAL_UART_Receive(context->handle,
	                        (uint8_t*) pvBuffer,
	                        (uint16_t) xBytes,
	                        transferParams->Timeout);
}

HAL_StatusTypeDef DrvUart_Write(void* vpParam, const void* pvBuffer, const uint32_t xBytes)
{
	const DrvUart_TransferParams_t* transferParams = (const DrvUart_TransferParams_t*) vpParam;
	DrvUart_Context_t* context = NULL;
	DrvUart_Mode_t txMode = DRV_UART_MODE_BLOCKING;

	if ((transferParams == NULL) || (pvBuffer == NULL) || (xBytes == 0U) || (xBytes > 0xFFFFU))
	{
		return HAL_ERROR;
	}

	context = DrvUart_GetContext(transferParams->UartIndex);
	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	txMode = context->openParams.TxMode;
	if ((txMode != DRV_UART_MODE_BLOCKING) &&
	    (DrvUart_EnableInterrupts(context,
	                              txMode,
	                              context->handle->hdmatx,
	                              context->txDmaOpenParams.DMAIndex) != HAL_OK))
	{
		return HAL_ERROR;
	}

	switch (txMode)
	{
	case DRV_UART_MODE_DMA:
		return HAL_UART_Transmit_DMA(context->handle, (uint8_t*) pvBuffer, (uint16_t) xBytes);

	case DRV_UART_MODE_INTERRUPT:
		return HAL_UART_Transmit_IT(context->handle, (uint8_t*) pvBuffer, (uint16_t) xBytes);

	case DRV_UART_MODE_BLOCKING:
	default:
		return HAL_UART_Transmit(context->handle,
		                         (uint8_t*) pvBuffer,
		                         (uint16_t) xBytes,
		                         transferParams->Timeout);
	}
}

static HAL_StatusTypeDef DrvUart_AbortInternal(uint32_t index)
{
	DrvUart_Context_t* context = DrvUart_GetContext(index);

	if ((context == NULL) || (context->handle == NULL))
	{
		return HAL_ERROR;
	}

	return HAL_UART_Abort(context->handle);
}

HAL_StatusTypeDef DrvUart_IrqHandler(void)
{
	IRQn_Type activeIrq = NonMaskableInt_IRQn;
	uint32_t exceptionNumber = __get_IPSR() & 0x1FFU;
	uint32_t index = 0U;

	if (exceptionNumber < 16U)
	{
		return HAL_ERROR;
	}

	activeIrq = (IRQn_Type) ((int32_t) exceptionNumber - 16);

	for (index = 0U; index < DRV_UART_MAX_INSTANCES; ++index)
	{
		DrvUart_Context_t* context = &g_uartContexts[index];

		if ((context->handle == NULL) ||
		    (context->openParams.Interrupt.Enabled == 0U) ||
		    (context->openParams.Interrupt.IRQn != activeIrq))
		{
			continue;
		}

		HAL_UART_IRQHandler(context->handle);
		DrvUart_ProcessRxEvent(context);
		return HAL_OK;
	}

	return HAL_ERROR;
}

HAL_StatusTypeDef DrvUart_Ioctl(const uint32_t xCommand, void* vpParam)
{
	switch ((DrvUart_IoctlCmd_t) xCommand)
	{
	case DRV_UART_IOCTL_ABORT:
	{
		const DrvUart_DeviceParams_t* deviceParams = (const DrvUart_DeviceParams_t*) vpParam;

		if (deviceParams == NULL)
		{
			return HAL_ERROR;
		}

		return DrvUart_AbortInternal(deviceParams->UartIndex);
	}

	case DRV_UART_IOCTL_REGISTER_CALLBACK:
	{
		const DrvUart_CallbackRequest_t* callbackRequest = (const DrvUart_CallbackRequest_t*) vpParam;
		DrvUart_Context_t* context = NULL;

		if ((callbackRequest == NULL) || (callbackRequest->UartIndex >= DRV_UART_MAX_INSTANCES))
		{
			return HAL_ERROR;
		}

		context = DrvUart_GetContext(callbackRequest->UartIndex);
		if (context == NULL)
		{
			return HAL_ERROR;
		}

		context->callback = callbackRequest->Callback;
		context->callbackUserData = callbackRequest->UserData;
		return HAL_OK;
	}

	case DRV_UART_IOCTL_ATTACH_INTERRUPT:
		return DrvUart_AttachInterruptInternal((const DrvUart_AttachInterruptRequest_t*) vpParam);

	default:
		return HAL_ERROR;
	}
}
