#include "transport/SerialTransport.h"

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
#ifndef _WIN32
speed_t toBaud(unsigned baudRate)
{
    switch (baudRate)
    {
    case 9600:
        return B9600;
    case 57600:
        return B57600;
    case 115200:
    default:
        return B115200;
    }
}
#endif
}

namespace transport
{
SerialTransport::SerialTransport()
#ifdef _WIN32
    : handle_(INVALID_HANDLE_VALUE)
#else
    : fd_(-1)
#endif
{
}

SerialTransport::~SerialTransport()
{
    close();
}

bool SerialTransport::open(const std::string& portName, unsigned baudRate, std::string& error)
{
    close();

#ifdef _WIN32
    std::string fullPortName = portName.rfind("\\\\.\\", 0U) == 0U ? portName : "\\\\.\\" + portName;
    handle_ = CreateFileA(fullPortName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE)
    {
        error = "failed to open serial port";
        return false;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb))
    {
        error = "failed to read serial configuration";
        close();
        return false;
    }

    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(handle_, &dcb))
    {
        error = "failed to configure serial port";
        close();
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 20;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    SetCommTimeouts(handle_, &timeouts);
    return true;
#else
    fd_ = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd_ < 0)
    {
        error = "failed to open serial port";
        return false;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0)
    {
        error = "failed to read serial configuration";
        close();
        return false;
    }

    cfsetospeed(&tty, toBaud(baudRate));
    cfsetispeed(&tty, toBaud(baudRate));
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_iflag = 0;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0)
    {
        error = "failed to apply serial configuration";
        close();
        return false;
    }

    return true;
#endif
}

void SerialTransport::close()
{
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

bool SerialTransport::isOpen() const
{
#ifdef _WIN32
    return handle_ != INVALID_HANDLE_VALUE;
#else
    return fd_ >= 0;
#endif
}

bool SerialTransport::write(const uint8_t* data, size_t length, std::string& error)
{
    if (!isOpen())
    {
        error = "serial transport is not open";
        return false;
    }

#ifdef _WIN32
    DWORD written = 0;
    const BOOL ok = WriteFile(handle_, data, static_cast<DWORD>(length), &written, nullptr);
    if (!ok || written != length)
    {
        error = "failed to write full frame";
        return false;
    }
#else
    const ssize_t written = ::write(fd_, data, length);
    if (written != static_cast<ssize_t>(length))
    {
        error = "failed to write full frame";
        return false;
    }
#endif
    return true;
}

size_t SerialTransport::read(uint8_t* buffer, size_t bufferLength, std::string& error)
{
    if (!isOpen())
    {
        error = "serial transport is not open";
        return 0U;
    }

#ifdef _WIN32
    DWORD readBytes = 0;
    if (!ReadFile(handle_, buffer, static_cast<DWORD>(bufferLength), &readBytes, nullptr))
    {
        error = "serial read failed";
        return 0U;
    }
    return static_cast<size_t>(readBytes);
#else
    const ssize_t readBytes = ::read(fd_, buffer, bufferLength);
    if (readBytes < 0)
    {
        return 0U;
    }
    return static_cast<size_t>(readBytes);
#endif
}

std::string SerialTransport::name() const
{
    return "serial";
}
}
