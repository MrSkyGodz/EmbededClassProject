#pragma once

#include "protocol/Frame.h"

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
    uint8_t header;
    std::vector<MessageField> fields;
};

class MessageRegistry
{
public:
    MessageRegistry();

    const std::vector<MessageDescription>& descriptions() const;
    bool buildPayload(const std::string& type,
                      const std::map<std::string, double>& values,
                      std::vector<uint8_t>& payload,
                      std::string& error) const;

private:
    std::vector<MessageDescription> descriptions_;
};
}
