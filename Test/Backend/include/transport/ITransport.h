#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace transport
{
class ITransport
{
public:
    virtual ~ITransport() = default;

    virtual bool open(const std::string& portName, unsigned baudRate, std::string& error) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool write(const uint8_t* data, size_t length, std::string& error) = 0;
    virtual size_t read(uint8_t* buffer, size_t bufferLength, std::string& error) = 0;
    virtual std::string name() const = 0;
};
}
