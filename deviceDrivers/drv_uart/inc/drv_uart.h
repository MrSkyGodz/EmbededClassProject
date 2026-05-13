#ifndef DRV_UART_H_
#define DRV_UART_H_

#include <stddef.h>
#include <stdint.h>
#include "drv_gpio/inc/drv_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRV_UART_MAX_INSTANCES 4U

typedef enum
{
	DRV_UART_MODE_BLOCKING = 0,
	DRV_UART_MODE_INTERRUPT,
	DRV_UART_MODE_DMA
} DrvUart_Mode_t;

typedef struct
{
	USART_TypeDef* Instance;
	uint32_t BaudRate;
	uint32_t WordLength;
	uint32_t StopBits;
	uint32_t Parity;
	uint32_t Mode;
	uint32_t HwFlowCtl;
	uint32_t OverSampling;
	uint32_t OneBitSampling;
	uint32_t AdvancedFeatureInit;
} DrvUart_PeripheralInit_t;

typedef struct
{
	uint8_t Enabled;
	IRQn_Type IRQn;
	uint32_t Priority;
	uint32_t SubPriority;
} DrvUart_InterruptInit_t;

typedef struct
{
	uint32_t DmaIndex;
	DMA_Stream_TypeDef* Instance;
	uint32_t Channel;
	IRQn_Type IRQn;
} DrvUart_DmaConfig_t;

typedef enum
{
	DRV_UART_EVENT_DATA = 0,
	DRV_UART_EVENT_IDLE,
	DRV_UART_EVENT_BUFFER_FULL
} DrvUart_Event_t;

typedef void (*DrvUart_Callback_t)(DrvUart_Event_t event,
	                               const uint8_t* rxBuffer,
	                               size_t rxLength,
	                               void* userData);

typedef struct
{
	uint32_t UartIndex;
	DrvUart_PeripheralInit_t UART_Init;
	DrvGpio_OpenParams_t GPIO_TXInit;
	DrvGpio_OpenParams_t GPIO_RXInit;
	DrvUart_InterruptInit_t Interrupt;
	DrvUart_DmaConfig_t RxDmaConfig;
	DrvUart_DmaConfig_t TxDmaConfig;
	DrvUart_Mode_t RxMode;
	DrvUart_Mode_t TxMode;
	UART_HandleTypeDef* UARTHandle;
} DrvUart_OpenParams_t;

typedef struct
{
	uint32_t UartIndex;
} DrvUart_DeviceParams_t;

typedef struct
{
	uint32_t UartIndex;
	uint32_t Timeout;
} DrvUart_TransferParams_t;

typedef struct
{
	uint32_t UartIndex;
	DrvUart_Callback_t Callback;
	void* UserData;
} DrvUart_CallbackRequest_t;

typedef struct
{
	uint32_t UartIndex;
	void* Buffer;
	size_t BufferLength;
} DrvUart_AttachInterruptRequest_t;

typedef enum
{
	DRV_UART_IOCTL_ABORT = 0,
	DRV_UART_IOCTL_REGISTER_CALLBACK,
	DRV_UART_IOCTL_ATTACH_INTERRUPT
} DrvUart_IoctlCmd_t;

HAL_StatusTypeDef DrvUart_Open(void* vpParam);
HAL_StatusTypeDef DrvUart_Close(void* vpParam);
HAL_StatusTypeDef DrvUart_Read(void* vpParam, void* pvBuffer, const uint32_t xBytes);
HAL_StatusTypeDef DrvUart_Write(void* vpParam, const void* pvBuffer, const uint32_t xBytes);
HAL_StatusTypeDef DrvUart_IrqHandler(void);
HAL_StatusTypeDef DrvUart_Ioctl(const uint32_t xCommand, void* vpParam);

#ifdef __cplusplus
}
#endif

#endif
