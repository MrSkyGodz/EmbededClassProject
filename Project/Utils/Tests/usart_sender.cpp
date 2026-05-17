#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "IcdMessageCodec.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
bool parseAngle(const char *text, float &value)
{
    char *end = nullptr;
    const float parsed = std::strtof(text, &end);

    if ((end == text) || (*end != '\0') || (parsed < 0.0f) || (parsed > 180.0f))
    {
        return false;
    }

    value = parsed;
    return true;
}

bool parseByte(const char *text, uint8_t &value)
{
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 0);

    if ((end == text) || (*end != '\0') || (parsed > 0xFFUL))
    {
        return false;
    }

    value = static_cast<uint8_t>(parsed);
    return true;
}

void printUsage(const char *programName)
{
    std::fprintf(stderr, "Usage:\n");
    std::fprintf(stderr, "  %s <port> pwm <0-255>\n", programName);
    std::fprintf(stderr, "  %s <port> motor <motor1_angle_deg> [motor2_angle_deg]\n", programName);
    std::fprintf(stderr, "Examples:\n");
    std::fprintf(stderr, "  %s COM5 pwm 128\n", programName);
    std::fprintf(stderr, "  %s COM5 motor 90.0 45.0\n", programName);
}

bool configurePwmPacket(int argc, char **argv, IcdMessage_t &packet)
{
    uint8_t pwmValue = 0U;

    if (argc < 4)
    {
        std::fprintf(stderr, "Missing pwm value\n");
        return false;
    }

    if (!parseByte(argv[3], pwmValue))
    {
        std::fprintf(stderr, "Invalid pwm value: %s\n", argv[3]);
        return false;
    }

    packet.Header.IcdType = IcdType_PWMControl;
    packet.Payload.PWMControl.Pwm = pwmValue;

    std::printf("ICD Type: %u, PWM: %u\n",
                static_cast<unsigned>(packet.Header.IcdType),
                static_cast<unsigned>(packet.Payload.PWMControl.Pwm));

    return true;
}

bool configureMotorPacket(int argc, char **argv, IcdMessage_t &packet)
{
    float motor1AngleDeg = 0.0f;
    float motor2AngleDeg = 0.0f;

    if (argc < 4)
    {
        std::fprintf(stderr, "Missing motor 1 angle value\n");
        return false;
    }

    if (!parseAngle(argv[3], motor1AngleDeg))
    {
        std::fprintf(stderr, "Invalid motor 1 angle value: %s\n", argv[3]);
        return false;
    }

    motor2AngleDeg = motor1AngleDeg;

    if (argc >= 5)
    {
        if (!parseAngle(argv[4], motor2AngleDeg))
        {
            std::fprintf(stderr, "Invalid motor 2 angle value: %s\n", argv[4]);
            return false;
        }
    }

    packet.Header.IcdType = IcdType_MotorControl;
    packet.Payload.MotorControl.Motor1AngleDeg = motor1AngleDeg;
    packet.Payload.MotorControl.Motor2AngleDeg = motor2AngleDeg;

    std::printf("ICD Type: %u, Motor1: %.2f deg, Motor2: %.2f deg\n",
                static_cast<unsigned>(packet.Header.IcdType),
                static_cast<double>(packet.Payload.MotorControl.Motor1AngleDeg),
                static_cast<double>(packet.Payload.MotorControl.Motor2AngleDeg));

    return true;
}

bool configurePacket(int argc, char **argv, IcdMessage_t &packet)
{
    if (std::strcmp(argv[2], "pwm") == 0)
    {
        return configurePwmPacket(argc, argv, packet);
    }

    if (std::strcmp(argv[2], "motor") == 0)
    {
        return configureMotorPacket(argc, argv, packet);
    }

    std::fprintf(stderr, "Unknown communication type: %s\n", argv[2]);
    return false;
}

uint8_t buildPayload(const IcdMessage_t &packet, uint8_t *payload)
{
    return IcdMessageCodec::BuildPayload(packet, payload);
}

void printFrame(const uint8_t *frame, size_t length)
{
    std::printf("Sending frame:");

    for (size_t i = 0U; i < length; ++i)
    {
        std::printf(" %02X", frame[i]);
    }

    std::printf("\n");
}

#ifdef _WIN32
bool sendFrameToPort(const char *portName, const uint8_t *frame, size_t length)
{
    char fullPortName[64];
    std::snprintf(fullPortName, sizeof(fullPortName), "\\\\.\\%s", portName);

    HANDLE handle = CreateFileA(fullPortName,
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        std::fprintf(stderr, "Failed to open port %s\n", portName);
        return false;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(handle, &dcb))
    {
        std::fprintf(stderr, "Failed to read serial config\n");
        CloseHandle(handle);
        return false;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(handle, &dcb))
    {
        std::fprintf(stderr, "Failed to configure serial port\n");
        CloseHandle(handle);
        return false;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(handle, frame, static_cast<DWORD>(length), &written, nullptr);
    CloseHandle(handle);

    if (!ok || (written != length))
    {
        std::fprintf(stderr, "Failed to write full frame to %s\n", portName);
        return false;
    }

    return true;
}
#else
bool sendFrameToPort(const char *portName, const uint8_t *frame, size_t length)
{
    const int fd = open(portName, O_WRONLY | O_NOCTTY | O_SYNC);

    if (fd < 0)
    {
        std::perror("open");
        return false;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0)
    {
        std::perror("tcgetattr");
        close(fd);
        return false;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_iflag = 0;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        std::perror("tcsetattr");
        close(fd);
        return false;
    }

    const ssize_t written = write(fd, frame, length);
    close(fd);

    if (written != static_cast<ssize_t>(length))
    {
        std::perror("write");
        return false;
    }

    return true;
}
#endif
}

int main(int argc, char **argv)
{
    static_assert(sizeof(IcdMessage_t) <= 64U, "ICD payload is larger than sender buffer");

    IcdMessage_t packet{};
    uint8_t frame[2U + 1U + sizeof(IcdMessage_t) + 1U];
    uint8_t payload[ICD_HEADER_SIZE_BYTES + sizeof(IcdPayload_u)] = {};
    size_t frameLength = 0U;

    if (argc < 3)
    {
        printUsage(argv[0]);
        return 1;
    }

    if (!configurePacket(argc, argv, packet))
    {
        printUsage(argv[0]);
        return 1;
    }

    const uint8_t payloadLength = buildPayload(packet, payload);
    if (payloadLength == 0U)
    {
        std::fprintf(stderr, "Failed to build payload\n");
        return 1;
    }

    frameLength = IcdProtocol::BuildFrame(payload, payloadLength, frame);
    printFrame(frame, frameLength);

    if (!sendFrameToPort(argv[1], frame, frameLength))
    {
        return 1;
    }

    std::puts("Frame sent successfully.");
    return 0;
}
