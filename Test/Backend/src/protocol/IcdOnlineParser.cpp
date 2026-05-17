#include "protocol/IcdOnlineParser.h"

namespace protocol
{
IcdOnlineParser::IcdOnlineParser(MessageRegistry& registry,
                                 recorder::ParsedMessageRecorder& recorder,
                                 ErrorCallback onError)
    : OnlineParser(2U, 128U),
      registry_(registry),
      recorder_(recorder),
      onError_(onError)
{
}

bool IcdOnlineParser::checkHeader(size_t headerIndex, uint8_t byte) const
{
    if (headerIndex == 0U)
    {
        return byte == 0xAAU;
    }
    if (headerIndex == 1U)
    {
        return byte == 0x55U;
    }
    return false;
}

bool IcdOnlineParser::checkCrc(const uint8_t* payload, uint8_t payloadLength, uint8_t crcByte) const
{
    uint8_t calculated = payloadLength;
    for (uint8_t i = 0U; i < payloadLength; ++i)
    {
        calculated ^= payload[i];
    }
    return calculated == crcByte;
}

void IcdOnlineParser::onFrameReceived(const uint8_t* payload, uint8_t payloadLength)
{
    DecodedMessage message;
    std::string error;
    if (!registry_.decodePayload(payload, payloadLength, message, error))
    {
        if (onError_)
        {
            onError_("ICD decode failed: " + error);
        }
        return;
    }

    recorder_.append(message.timetagMs, message.counter, message.icdType, message.type, message.values);
}
}
