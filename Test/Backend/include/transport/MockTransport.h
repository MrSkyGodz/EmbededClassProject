#pragma once

#include "transport/ITransport.h"

#include <mutex>
#include <queue>

namespace transport
{
class MockTransport : public ITransport
{
public:
    bool open(const std::string& portName, unsigned baudRate, std::string& error) override;
    void close() override;
    bool isOpen() const override;
    bool write(const uint8_t* data, size_t length, std::string& error) override;
    size_t read(uint8_t* buffer, size_t bufferLength, std::string& error) override;
    std::string name() const override;

private:
    mutable std::mutex mutex_;
    std::queue<uint8_t> rxQueue_;
    bool open_ = false;
    uint8_t generated_ = 0U;
};
}
