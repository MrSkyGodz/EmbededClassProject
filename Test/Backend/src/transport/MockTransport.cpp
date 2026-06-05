#include "transport/MockTransport.h"

#include "protocol/Frame.h"

#include <cstring>
#include <thread>
#include <vector>

namespace
{
constexpr uint8_t IcdType_Bno055Telemetry = 2U;
constexpr uint8_t IcdType_Bno055CalibrationStatus = 6U;

void appendUint32Le(std::vector<uint8_t>& payload, uint32_t value)
{
    payload.push_back(static_cast<uint8_t>(value & 0xFFU));
    payload.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    payload.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    payload.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
}

void appendFloat(std::vector<uint8_t>& payload, float value)
{
    uint8_t raw[sizeof(float)] = {};
    std::memcpy(raw, &value, sizeof(float));
    payload.insert(payload.end(), raw, raw + sizeof(float));
}
}

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
    (void) data;
    (void) length;
    (void) error;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_)
    {
        error = "mock transport is not open";
        return false;
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
        const uint8_t counter = generated_++;
        const float step = static_cast<float>(counter);
        std::vector<uint8_t> payload;
        appendUint32Le(payload, static_cast<uint32_t>(counter) * 20U);
        payload.push_back(counter);
        payload.push_back(IcdType_Bno055Telemetry);
        appendFloat(payload, 10.0F + (step * 0.5F));
        appendFloat(payload, -3.0F + (step * 0.25F));
        appendFloat(payload, 45.0F + step);
        appendFloat(payload, 0.01F * step);
        appendFloat(payload, 0.02F * step);
        appendFloat(payload, 0.03F * step);

        const auto frame = protocol::buildFrame(payload);
        for (uint8_t byte : frame.bytes)
        {
            rxQueue_.push(byte);
        }

        std::vector<uint8_t> calibrationPayload;
        appendUint32Le(calibrationPayload, static_cast<uint32_t>(counter) * 20U);
        calibrationPayload.push_back(counter);
        calibrationPayload.push_back(IcdType_Bno055CalibrationStatus);
        calibrationPayload.push_back(3U);
        calibrationPayload.push_back(3U);
        calibrationPayload.push_back(3U);
        calibrationPayload.push_back(3U);
        calibrationPayload.push_back(1U);

        const auto calibrationFrame = protocol::buildFrame(calibrationPayload);
        for (uint8_t byte : calibrationFrame.bytes)
        {
            rxQueue_.push(byte);
        }
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
