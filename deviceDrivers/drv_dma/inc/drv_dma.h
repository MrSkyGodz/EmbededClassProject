#ifndef DRV_DMA_H_
#define DRV_DMA_H_

#include <stdint.h>
#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_DMA_MAX_INSTANCES 8U

typedef struct
{
	DMA_Stream_TypeDef* Instance;
	uint32_t Channel;
	uint32_t Direction;
	uint32_t PeriphInc;
	uint32_t MemInc;
	uint32_t PeriphDataAlignment;
	uint32_t MemDataAlignment;
	uint32_t Mode;
	uint32_t Priority;
	uint32_t FIFOMode;
	uint32_t FIFOThreshold;
	uint32_t MemBurst;
	uint32_t PeriphBurst;
} DrvDma_PeripheralInit_t;

typedef struct
{
	uint8_t Enabled;
	IRQn_Type IRQn;
	uint32_t Priority;
	uint32_t SubPriority;
} DrvDma_InterruptInit_t;

typedef void (*DrvDma_Callback_t)(void* userData);

typedef struct
{
	uint32_t DMAIndex;
	DrvDma_PeripheralInit_t DMA_Init;
	DrvDma_InterruptInit_t Interrupt;
	DMA_HandleTypeDef* DMAHandle;
	DrvDma_Callback_t Callback;
	void* CallbackUserData;
} DrvDma_OpenParams_t;

typedef struct
{
	uint32_t DMAIndex;
} DrvDma_DeviceParams_t;

HAL_StatusTypeDef DrvDma_Open(void* vpParam);
HAL_StatusTypeDef DrvDma_Close(void* vpParam);
DMA_HandleTypeDef* DrvDma_GetHandle(uint32_t index);
HAL_StatusTypeDef DrvDma_EnableInterrupt(uint32_t index);
HAL_StatusTypeDef DrvDma_DisableInterrupt(uint32_t index);
HAL_StatusTypeDef DrvDma_Start(uint32_t index,
                               uint32_t srcAddress,
                               uint32_t dstAddress,
                               uint32_t dataLength);
HAL_StatusTypeDef DrvDma_StartIT(uint32_t index,
                                 uint32_t srcAddress,
                                 uint32_t dstAddress,
                                 uint32_t dataLength);
HAL_StatusTypeDef DrvDma_Abort(uint32_t index);
HAL_StatusTypeDef DrvDma_IrqHandler(void);

#ifdef __cplusplus
}
#endif

#endif
