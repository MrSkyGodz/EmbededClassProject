#pragma once

#include "OnlineParser.hpp"
#include "protocol/MessageRegistry.h"
#include "recorder/ParsedMessageRecorder.h"

#include <functional>
#include <string>

namespace protocol
{
class IcdOnlineParser : public OnlineParser
{
public:
    using ErrorCallback = std::function<void(const std::string&)>;

    IcdOnlineParser(MessageRegistry& registry,
                    recorder::ParsedMessageRecorder& recorder,
                    ErrorCallback onError);

protected:
    bool checkHeader(size_t headerIndex, uint8_t byte) const override;
    bool checkCrc(const uint8_t* payload, uint8_t payloadLength, uint8_t crcByte) const override;
    void onFrameReceived(const uint8_t* payload, uint8_t payloadLength) override;

private:
    MessageRegistry& registry_;
    recorder::ParsedMessageRecorder& recorder_;
    ErrorCallback onError_;
};
}
