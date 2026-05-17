#pragma once

#include "protocol/Frame.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace protocol
{
struct MessageField
{
    std::string name;
    std::string type;
    double minValue;
    double maxValue;
};

struct MessageDescription
{
    std::string type;
    uint8_t icdType;
    std::string direction;
    std::vector<MessageField> fields;
};

struct DecodedMessage
{
    uint32_t timetagMs = 0U;
    uint8_t counter = 0U;
    uint8_t icdType = 0U;
    std::string type;
    std::map<std::string, double> values;
};

class MessageRegistry
{
public:
    MessageRegistry();

    const std::vector<MessageDescription>& descriptions() const;
    const MessageDescription* findByType(const std::string& type) const;
    bool buildPayload(const std::string& type,
                      const std::map<std::string, double>& values,
                      uint32_t timetagMs,
                      uint8_t counter,
                      std::vector<uint8_t>& payload,
                      std::string& error) const;
    bool decodePayload(const uint8_t* payload,
                       size_t payloadLength,
                       DecodedMessage& message,
                       std::string& error) const;

private:
    const MessageDescription* findByIcdType(uint8_t icdType) const;

    std::vector<MessageDescription> descriptions_;
};
}
