#pragma once
	
#include <stdint.h>
#include <string.h>

#include "CommunicationType.h"
#include "OnlineParser.hpp"

class ParserImp : public OnlineParser
{
public:
	ParserImp() : OnlineParser(2U, ICD_HEADER_SIZE_BYTES + sizeof(IcdPayload_u)) {}

	IcdMessage_t receivedCommand = {};
	volatile uint8_t commandReady = 0U;

protected:
    bool checkHeader(size_t headerIndex, uint8_t byte) const override
    {
    	static const uint8_t kFrameHeader[] = {0xAAU, 0x55U};

    	if (headerIndex >= sizeof(kFrameHeader))
    	{
    		return false;
    	}

    	return byte == kFrameHeader[headerIndex];
    }
	bool checkCrc(const uint8_t *payload, uint8_t payloadLength, uint8_t crcByte) const override
	{
		uint8_t calculatedCrc = payloadLength;

	for (uint8_t i = 0U; i < payloadLength; ++i)
	{
		calculatedCrc ^= payload[i];
	}

	return crcByte == calculatedCrc;
	}
	void onFrameReceived(const uint8_t *payload, uint8_t payloadLength) override
	{
		if (payloadLength < ICD_HEADER_SIZE_BYTES)
		{
			return;
		}

		const uint8_t* messagePayload = &payload[ICD_HEADER_SIZE_BYTES];
		const uint8_t messagePayloadLength = payloadLength - ICD_HEADER_SIZE_BYTES;

		receivedCommand.Header.TimetagMs = static_cast<uint32_t>(payload[0]) |
		                                   (static_cast<uint32_t>(payload[1]) << 8U) |
		                                   (static_cast<uint32_t>(payload[2]) << 16U) |
		                                   (static_cast<uint32_t>(payload[3]) << 24U);
		receivedCommand.Header.Counter = payload[4];
		receivedCommand.Header.IcdType = payload[5];

		if (receivedCommand.Header.IcdType == IcdType_PWMControl)
		{
			if (messagePayloadLength != sizeof(receivedCommand.Payload.PWMControl))
			{
				return;
			}

			memcpy((void *) &receivedCommand.Payload.PWMControl, messagePayload, sizeof(receivedCommand.Payload.PWMControl));
		}
		else if (receivedCommand.Header.IcdType == IcdType_MotorControl)
		{
			if (messagePayloadLength != sizeof(receivedCommand.Payload.MotorControl))
			{
				return;
			}

			memcpy((void *) &receivedCommand.Payload.MotorControl, messagePayload, sizeof(receivedCommand.Payload.MotorControl));
		}
		else
		{
			return;
		}

		commandReady = 1U;
	}
};
