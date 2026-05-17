#include "recorder/ParsedMessageRecorder.h"

#include <chrono>

namespace recorder
{
ParsedMessageRecorder::ParsedMessageRecorder(size_t capacityMessages)
    : capacityMessages_(capacityMessages)
{
}

void ParsedMessageRecorder::append(uint32_t timetagMs,
                                   uint8_t counter,
                                   uint8_t icdType,
                                   const std::string& type,
                                   const std::map<std::string, double>& values)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (messages_.size() >= capacityMessages_)
    {
        messages_.pop_front();
    }

    messages_.push_back({nextIndex_++, nowMs(), timetagMs, counter, icdType, type, values});
}

void ParsedMessageRecorder::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.clear();
}

std::vector<ParsedMessage> ParsedMessageRecorder::snapshot(size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (maxCount == 0U || maxCount >= messages_.size())
    {
        return std::vector<ParsedMessage>(messages_.begin(), messages_.end());
    }

    return std::vector<ParsedMessage>(messages_.end() - static_cast<std::ptrdiff_t>(maxCount), messages_.end());
}

size_t ParsedMessageRecorder::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return messages_.size();
}

uint64_t ParsedMessageRecorder::totalParsed() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return nextIndex_;
}

uint64_t ParsedMessageRecorder::nowMs()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}
}
