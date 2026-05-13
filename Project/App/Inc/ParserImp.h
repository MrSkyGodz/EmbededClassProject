#pragma once

#include <stdint.h>
#include <string.h>

#include "CommunicationType.h"
#include "OnlineParser.hpp"
#include "Security.h"   /* Week 7 — upgraded CRC-8/MAXIM integrity check */

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

    /*
     * Week 7 Security — Integrity:
     * Replaced the original XOR-accumulate CRC with CRC-8/MAXIM (poly 0x31).
     * The XOR approach fails silently on even-count multi-bit errors in the
     * same bit position. CRC-8 detects all 1-bit and 2-bit errors and has
     * a Hamming distance of 4 for payloads ≤ 8 bytes.
     *
     * The sender (PC script) must now compute CRC-8/MAXIM over the payload
     * bytes (excluding the 2-byte header and 1-byte length) and append it.
     */
	bool checkCrc(const uint8_t *payload, uint8_t payloadLength, uint8_t crcByte) const override
	{
		const uint8_t calculatedCrc = SEC_Crc8(payload, static_cast<size_t>(payloadLength));
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
