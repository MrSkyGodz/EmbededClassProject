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

	static float readFloat(const uint8_t* payload)
	{
		float value = 0.0F;
		memcpy(&value, payload, sizeof(value));
		return value;
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
		else if (receivedCommand.Header.IcdType == IcdType_ImuReferenceControl)
		{
			const uint8_t expectedLength = (sizeof(float) * 2U) + 2U;
			if (messagePayloadLength != expectedLength)
			{
				return;
			}

			receivedCommand.Payload.ImuReferenceControl.TargetAzimuthDeg = readFloat(messagePayload);
			receivedCommand.Payload.ImuReferenceControl.TargetElevationDeg = readFloat(messagePayload + sizeof(float));
			receivedCommand.Payload.ImuReferenceControl.Enable = messagePayload[sizeof(float) * 2U];
			receivedCommand.Payload.ImuReferenceControl.FrameMode = messagePayload[(sizeof(float) * 2U) + 1U];
		}
		else if (receivedCommand.Header.IcdType == IcdType_ImuReferenceTuning)
		{
			const uint8_t expectedLength = (sizeof(float) * 4U) + 1U;
			if (messagePayloadLength != expectedLength)
			{
				return;
			}

			receivedCommand.Payload.ImuReferenceTuning.AzimuthKp = readFloat(messagePayload);
			receivedCommand.Payload.ImuReferenceTuning.AzimuthKi = readFloat(messagePayload + sizeof(float));
			receivedCommand.Payload.ImuReferenceTuning.ElevationKp = readFloat(messagePayload + (sizeof(float) * 2U));
			receivedCommand.Payload.ImuReferenceTuning.ElevationKi = readFloat(messagePayload + (sizeof(float) * 3U));
			receivedCommand.Payload.ImuReferenceTuning.ResetIntegrator = messagePayload[sizeof(float) * 4U];
		}
		else
		{
			return;
		}

		commandReady = 1U;
	}
};
