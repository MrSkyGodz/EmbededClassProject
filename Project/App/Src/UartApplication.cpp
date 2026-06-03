/*
 * UartApplication.cpp
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */

#include "usart.h"
#include "drv_uart/inc/drv_uart.h"
#include "CommunicationApplication.h"



bool WriteIcdFrame(void* context, const uint8_t* frame, uint8_t frameLength)
{
	DrvUart_TransferParams_t* transfer = static_cast<DrvUart_TransferParams_t*>(context);
	if ((transfer == nullptr) || (frame == nullptr) || (frameLength == 0U))
	{
		return false;
	}

	return DrvUart_Write(transfer, frame, frameLength) == HAL_OK;
}

bool IsIcdFrameTxBusy(void* context)
{
	const DrvUart_TransferParams_t* transfer = static_cast<const DrvUart_TransferParams_t*>(context);
	if (transfer == nullptr)
	{
		return false;
	}

	return DrvUart_IsTxBusy(transfer->UartIndex) != 0U;
}


void DebugUartDataCallback(DrvUart_Event_t event,
								  const uint8_t* rxBuffer,
								  size_t rxLength,
								  void* userData)
{
	(void) event;
	(void) userData;

	for (size_t i = 0U; i < rxLength; ++i)
	{
		parser.pushByte(rxBuffer[i]);
	}
}


DrvUart_TransferParams_t DebugUartTransfer = {.UartIndex = USART3_UART_INDEX, .Timeout = HAL_MAX_DELAY};

void* GetDebugUartTransfer()
{
	return &DebugUartTransfer;
}

DrvUart_CallbackRequest_t DebugUartCallbackRequest = {.UartIndex = USART3_UART_INDEX,
                                                      .Callback = DebugUartDataCallback,
                                                      .UserData = nullptr};

static uint8_t DebugUartRxBuffer[64U] = {0U};


DrvUart_AttachInterruptRequest_t DebugUartAttachRequest = {.UartIndex = USART3_UART_INDEX,
                                                           .Buffer = DebugUartRxBuffer,
                                                           .BufferLength = sizeof(DebugUartRxBuffer)};


void UartStart()
{
	if ((DrvUart_Ioctl(DRV_UART_IOCTL_REGISTER_CALLBACK, &DebugUartCallbackRequest) != HAL_OK) ||
	    (DrvUart_Ioctl(DRV_UART_IOCTL_ATTACH_INTERRUPT, &DebugUartAttachRequest) != HAL_OK))
	{
		Error_Handler();
	}
}



extern "C" int _write(int file, char *ptr, int len)
{
  (void) file;

  if ((ptr == nullptr) || (len <= 0))
  {
    return 0;
  }

  return (DrvUart_Write(&DebugUartTransfer, ptr, (uint32_t) len) == HAL_OK) ? len : 0;
}

