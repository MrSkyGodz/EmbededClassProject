#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace protocol
{
struct Frame
{
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> payload;
};

uint8_t crc8(const std::vector<uint8_t>& payload);
Frame buildFrame(const std::vector<uint8_t>& payload);
std::string bytesToHex(const std::vector<uint8_t>& bytes);
bool startsWithFrameHeader(const std::vector<uint8_t>& bytes);
}
