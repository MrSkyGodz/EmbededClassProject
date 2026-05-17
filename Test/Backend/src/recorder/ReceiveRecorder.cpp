#include "recorder/ReceiveRecorder.h"

#include <chrono>
#include <fstream>
#include <iomanip>

namespace recorder
{
ReceiveRecorder::ReceiveRecorder(size_t capacityBytes)
    : capacityBytes_(capacityBytes)
{
}

void ReceiveRecorder::append(const uint8_t* data, size_t length)
{
    const uint64_t timestamp = nowMs();
    std::lock_guard<std::mutex> lock(mutex_);

    for (size_t i = 0U; i < length; ++i)
    {
        if (buffer_.size() >= capacityBytes_)
        {
            buffer_.pop_front();
        }

        buffer_.push_back({nextIndex_++, timestamp, data[i]});
    }
}

void ReceiveRecorder::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
}

std::vector<ReceivedByte> ReceiveRecorder::snapshot(size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (maxCount == 0U || maxCount >= buffer_.size())
    {
        return std::vector<ReceivedByte>(buffer_.begin(), buffer_.end());
    }

    return std::vector<ReceivedByte>(buffer_.end() - static_cast<std::ptrdiff_t>(maxCount), buffer_.end());
}

std::vector<uint8_t> ReceiveRecorder::snapshotValues(size_t maxCount) const
{
    const auto entries = snapshot(maxCount);
    std::vector<uint8_t> values;
    values.reserve(entries.size());
    for (const auto& entry : entries)
    {
        values.push_back(entry.value);
    }
    return values;
}

size_t ReceiveRecorder::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

uint64_t ReceiveRecorder::totalReceived() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return nextIndex_;
}

bool ReceiveRecorder::exportText(const std::string& path, std::string& error) const
{
    const auto entries = snapshot();
    std::ofstream file(path);
    if (!file)
    {
        error = "failed to open export file";
        return false;
    }

    file << "index,timestamp_ms,hex,ascii\n";
    file << std::uppercase << std::hex << std::setfill('0');
    for (const auto& entry : entries)
    {
        const char ascii = (entry.value >= 32U && entry.value <= 126U) ? static_cast<char>(entry.value) : '.';
        file << std::dec << entry.index << ',' << entry.timestampMs << ",0x"
             << std::hex << std::setw(2) << static_cast<unsigned>(entry.value)
             << std::dec << ',' << ascii << '\n';
    }

    return true;
}

uint64_t ReceiveRecorder::nowMs()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}
}
