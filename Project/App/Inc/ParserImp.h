#pragma once
	
#include <stdint.h>
#include <string.h>

#include "CommunicationType.h"
#include "OnlineParser.hpp"

class ParserImp : public OnlineParser
{
public:
	ParserImp() : OnlineParser(2U, sizeof(CommunicationProtocol_t)) {}

	CommunicationProtocol_t receivedCommand = {};
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
		if (payloadLength < sizeof(Header_e))
		{
			return;
		}

		const Header_e header = static_cast<Header_e>(payload[0]);
		const uint8_t* messagePayload = &payload[sizeof(Header_e)];
		const uint8_t messagePayloadLength = payloadLength - sizeof(Header_e);

		if (header == Header_PWMControl)
		{
			if (messagePayloadLength != sizeof(receivedCommand.Cp.PWMControl))
			{
				return;
			}

			receivedCommand.Header = header;
			memcpy((void *) &receivedCommand.Cp.PWMControl, messagePayload, sizeof(receivedCommand.Cp.PWMControl));
		}
		else if (header == Header_MotorControl)
		{
			if (messagePayloadLength != sizeof(receivedCommand.Cp.MotorControl))
			{
				return;
			}

			receivedCommand.Header = header;
			memcpy((void *) &receivedCommand.Cp.MotorControl, messagePayload, sizeof(receivedCommand.Cp.MotorControl));
		}
		else
		{
			return;
		}

		commandReady = 1U;
	}
};
