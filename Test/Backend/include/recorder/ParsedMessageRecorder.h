#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace recorder
{
struct ParsedMessage
{
    uint64_t index;
    uint64_t timestampMs;
    uint32_t timetagMs;
    uint8_t counter;
    uint8_t icdType;
    std::string type;
    std::map<std::string, double> values;
};

class ParsedMessageRecorder
{
public:
    explicit ParsedMessageRecorder(size_t capacityMessages = 4096U);

    void append(uint32_t timetagMs,
                uint8_t counter,
                uint8_t icdType,
                const std::string& type,
                const std::map<std::string, double>& values);
    void clear();

    std::vector<ParsedMessage> snapshot(size_t maxCount = 0U) const;
    size_t size() const;
    uint64_t totalParsed() const;

private:
    static uint64_t nowMs();

    mutable std::mutex mutex_;
    std::deque<ParsedMessage> messages_;
    size_t capacityMessages_;
    uint64_t nextIndex_ = 0U;
};
}
