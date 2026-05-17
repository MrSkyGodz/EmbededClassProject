#include "protocol/Frame.h"

#include <iomanip>
#include <sstream>

namespace protocol
{
uint8_t crc8(const std::vector<uint8_t>& payload)
{
    uint8_t crc = static_cast<uint8_t>(payload.size());

    for (uint8_t byte : payload)
    {
        crc ^= byte;
    }

    return crc;
}

Frame buildFrame(const std::vector<uint8_t>& payload)
{
    Frame frame;
    frame.payload = payload;
    frame.bytes.reserve(payload.size() + 4U);
    frame.bytes.push_back(0xAAU);
    frame.bytes.push_back(0x55U);
    frame.bytes.push_back(static_cast<uint8_t>(payload.size()));
    frame.bytes.insert(frame.bytes.end(), payload.begin(), payload.end());
    frame.bytes.push_back(crc8(payload));
    return frame;
}

std::string bytesToHex(const std::vector<uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');

    for (size_t i = 0U; i < bytes.size(); ++i)
    {
        if (i != 0U)
        {
            out << ' ';
        }

        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }

    return out.str();
}

bool startsWithFrameHeader(const std::vector<uint8_t>& bytes)
{
    return bytes.size() >= 2U && bytes[0] == 0xAAU && bytes[1] == 0x55U;
}
}
