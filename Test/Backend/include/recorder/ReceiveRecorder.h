#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace recorder
{
struct ReceivedByte
{
    uint64_t index;
    uint64_t timestampMs;
    uint8_t value;
};

class ReceiveRecorder
{
public:
    explicit ReceiveRecorder(size_t capacityBytes = 1024U * 1024U);

    void append(const uint8_t* data, size_t length);
    void clear();

    std::vector<ReceivedByte> snapshot(size_t maxCount = 0U) const;
    std::vector<uint8_t> snapshotValues(size_t maxCount = 0U) const;
    size_t size() const;
    uint64_t totalReceived() const;
    bool exportText(const std::string& path, std::string& error) const;

private:
    static uint64_t nowMs();

    mutable std::mutex mutex_;
    std::deque<ReceivedByte> buffer_;
    size_t capacityBytes_;
    uint64_t nextIndex_ = 0U;
};
}
