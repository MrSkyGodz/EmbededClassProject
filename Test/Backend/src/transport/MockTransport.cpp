#include "transport/MockTransport.h"

#include <thread>

namespace transport
{
bool MockTransport::open(const std::string& portName, unsigned baudRate, std::string& error)
{
    (void) portName;
    (void) baudRate;
    (void) error;
    std::lock_guard<std::mutex> lock(mutex_);
    open_ = true;
    generated_ = 0U;
    while (!rxQueue_.empty())
    {
        rxQueue_.pop();
    }
    return true;
}

void MockTransport::close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    open_ = false;
}

bool MockTransport::isOpen() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return open_;
}

bool MockTransport::write(const uint8_t* data, size_t length, std::string& error)
{
    (void) error;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_)
    {
        error = "mock transport is not open";
        return false;
    }

    for (size_t i = 0U; i < length; ++i)
    {
        rxQueue_.push(data[i]);
    }

    return true;
}

size_t MockTransport::read(uint8_t* buffer, size_t bufferLength, std::string& error)
{
    (void) error;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_ || bufferLength == 0U)
    {
        return 0U;
    }

    if (rxQueue_.empty())
    {
        rxQueue_.push(0xAAU);
        rxQueue_.push(0x55U);
        rxQueue_.push(0x01U);
        rxQueue_.push(generated_++);
        rxQueue_.push(static_cast<uint8_t>(0x01U ^ static_cast<uint8_t>(generated_ - 1U)));
    }

    size_t count = 0U;
    while (count < bufferLength && !rxQueue_.empty())
    {
        buffer[count++] = rxQueue_.front();
        rxQueue_.pop();
    }

    return count;
}

std::string MockTransport::name() const
{
    return "mock";
}
}
