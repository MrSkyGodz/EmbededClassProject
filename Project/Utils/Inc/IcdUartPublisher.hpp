#pragma once

#include "IcdMessageCodec.hpp"
#include "drv_uart/inc/drv_uart.h"

class IcdUartPublisher
{
public:
	explicit IcdUartPublisher(DrvUart_TransferParams_t& uartTransfer)
		: uartTransfer_(uartTransfer)
	{
	}

	bool Publish(const IcdMessage_t& message)
	{
		uint8_t frame[2U + 1U + ICD_HEADER_SIZE_BYTES + sizeof(IcdPayload_u) + 1U] = {0U};
		const uint8_t frameLength = IcdMessageCodec::BuildFrame(message, frame);
		if (frameLength == 0U)
		{
			return false;
		}

		return DrvUart_Write(&uartTransfer_, frame, frameLength) == HAL_OK;
	}

private:
	DrvUart_TransferParams_t& uartTransfer_;
};
