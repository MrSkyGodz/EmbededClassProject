#include "example_online_parser.hpp"

ExampleOnlineParser::ExampleOnlineParser()
    : OnlineParser(HeaderLength, MaxExamplePayload)
{
}

bool ExampleOnlineParser::checkHeader(size_t headerIndex, uint8_t byte) const
{
    if (headerIndex == 0U)
    {
        return byte == Header0;
    }

    if (headerIndex == 1U)
    {
        return byte == Header1;
    }

    return false;
}

bool ExampleOnlineParser::checkCrc(const uint8_t *payload, uint8_t payloadLength, uint8_t crcByte) const
{
    uint8_t calculatedCrc = payloadLength;

    for (uint8_t i = 0U; i < payloadLength; ++i)
    {
        calculatedCrc ^= payload[i];
    }

    return calculatedCrc == crcByte;
}
