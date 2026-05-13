#ifndef EXAMPLE_ONLINE_PARSER_HPP
#define EXAMPLE_ONLINE_PARSER_HPP

#include "OnlineParser.hpp"

class ExampleOnlineParser : public OnlineParser
{
public:
    ExampleOnlineParser();

protected:
    bool checkHeader(size_t headerIndex, uint8_t byte) const override;
    bool checkCrc(const uint8_t *payload, uint8_t payloadLength, uint8_t crcByte) const override;

private:
    static constexpr uint8_t Header0 = 0xAAU;
    static constexpr uint8_t Header1 = 0x55U;
    static constexpr uint8_t HeaderLength = 2U;
    static constexpr uint8_t MaxExamplePayload = 32U;
};

#endif
