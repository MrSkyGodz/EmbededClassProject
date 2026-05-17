#pragma once

#include <stdint.h>
#include <string.h>

#include "CommunicationType.h"

namespace IcdProtocol
{
static constexpr uint8_t FrameHeader0 = 0xAAU;
static constexpr uint8_t FrameHeader1 = 0x55U;
static constexpr uint8_t FrameOverheadBytes = 4U;

inline uint8_t CalculateCrc8(const uint8_t* payload, uint8_t payloadLength)
{
	uint8_t crc = payloadLength;
	for (uint8_t i = 0U; i < payloadLength; ++i)
	{
		crc ^= payload[i];
	}
	return crc;
}

inline void AppendUint32Le(uint8_t* buffer, uint8_t* offset, uint32_t value)
{
	buffer[(*offset)++] = static_cast<uint8_t>(value & 0xFFU);
	buffer[(*offset)++] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
	buffer[(*offset)++] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
	buffer[(*offset)++] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

inline void AppendFloat(uint8_t* buffer, uint8_t* offset, float value)
{
	memcpy(&buffer[*offset], &value, sizeof(float));
	*offset = static_cast<uint8_t>(*offset + sizeof(float));
}

inline uint8_t AppendHeader(uint8_t* payload,
                            uint32_t timetagMs,
                            uint8_t counter,
                            IcdType_e icdType)
{
	uint8_t offset = 0U;
	AppendUint32Le(payload, &offset, timetagMs);
	payload[offset++] = counter;
	payload[offset++] = static_cast<uint8_t>(icdType);
	return offset;
}

inline uint8_t BuildFrame(const uint8_t* payload, uint8_t payloadLength, uint8_t* frame)
{
	uint8_t offset = 0U;
	frame[offset++] = FrameHeader0;
	frame[offset++] = FrameHeader1;
	frame[offset++] = payloadLength;
	memcpy(&frame[offset], payload, payloadLength);
	offset = static_cast<uint8_t>(offset + payloadLength);
	frame[offset++] = CalculateCrc8(payload, payloadLength);
	return offset;
}
}
