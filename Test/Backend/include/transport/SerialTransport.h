#pragma once

#include "transport/ITransport.h"

namespace transport
{
class SerialTransport : public ITransport
{
public:
    SerialTransport();
    ~SerialTransport() override;

    bool open(const std::string& portName, unsigned baudRate, std::string& error) override;
    void close() override;
    bool isOpen() const override;
    bool write(const uint8_t* data, size_t length, std::string& error) override;
    size_t read(uint8_t* buffer, size_t bufferLength, std::string& error) override;
    std::string name() const override;

private:
#ifdef _WIN32
    void* handle_;
#else
    int fd_;
#endif
};
}
