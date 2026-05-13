#include "example_online_parser.hpp"
#include "ComLib.h"

#include <cstdio>
#include <cstring>

namespace
{
class RecordingParser : public ExampleOnlineParser
{
public:
    void expectFrame(const uint8_t *expectedPayload, uint8_t expectedLength)
    {
        expectedPayload_ = expectedPayload;
        expectedLength_ = expectedLength;
        frameReceived_ = false;
        frameMatched_ = false;
        callbackCount_ = 0U;
        lastPayloadLength_ = 0U;
    }

    bool frameReceived() const
    {
        return frameReceived_;
    }

    bool frameMatched() const
    {
        return frameMatched_;
    }

    uint8_t lastPayloadLength() const
    {
        return lastPayloadLength_;
    }

    uint32_t callbackCount() const
    {
        return callbackCount_;
    }

protected:
    void onFrameReceived(const uint8_t *payload, uint8_t payloadLength) override
    {
        frameReceived_ = true;
        frameMatched_ = false;
        callbackCount_++;
        lastPayloadLength_ = payloadLength;

        if (payloadLength != expectedLength_)
        {
            return;
        }

        if (payloadLength == 0U)
        {
            frameMatched_ = true;
            return;
        }

        frameMatched_ = (expectedPayload_ != nullptr) &&
                        (std::memcmp(payload, expectedPayload_, payloadLength) == 0);
    }

private:
    const uint8_t *expectedPayload_ = nullptr;
    uint8_t expectedLength_ = 0U;
    bool frameReceived_ = false;
    bool frameMatched_ = false;
    uint32_t callbackCount_ = 0U;
    uint8_t lastPayloadLength_ = 0U;
};

uint8_t crc8(const uint8_t *payload, uint8_t payloadLength)
{
    uint8_t crc = payloadLength;

    for (uint8_t i = 0U; i < payloadLength; ++i)
    {
        crc ^= payload[i];
    }

    return crc;
}

bool expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    return true;
}

bool expectPayload(const RecordingParser &parser, uint8_t length)
{
    if (!expect(parser.frameReceived(), "frame callback should run"))
    {
        return false;
    }

    if (!expect(parser.lastPayloadLength() == length, "payload length mismatch"))
    {
        return false;
    }

    if (!expect(parser.frameMatched(), "payload content mismatch"))
    {
        return false;
    }

    return true;
}

class MockTransport : public IComLib
{
public:
    bool Write(const uint8_t *buffer, size_t length) override
    {
        writeCalled = true;
        lastWriteBuffer = buffer;
        lastWriteLength = length;
        return writeResult;
    }

    size_t Read(uint8_t *buffer, size_t bufferLength) override
    {
        readCalled = true;
        lastReadBuffer = buffer;
        lastReadLength = bufferLength;

        if (!readShouldSucceed)
        {
            return 0U;
        }

        for (size_t i = 0U; i < bufferLength; ++i)
        {
            buffer[i] = static_cast<uint8_t>(0xA0U + i);
        }

        return bufferLength;
    }

    bool writeCalled = false;
    bool readCalled = false;
    bool writeResult = true;
    bool readShouldSucceed = true;
    const uint8_t *lastWriteBuffer = nullptr;
    uint8_t *lastReadBuffer = nullptr;
    size_t lastWriteLength = 0U;
    size_t lastReadLength = 0U;
};

bool testValidFrame()
{
    RecordingParser parser;
    const uint8_t payload[] = {0x10U, 0x20U, 0x30U};
    const uint8_t frame[] = {0xAAU, 0x55U, 0x03U, 0x10U, 0x20U, 0x30U, crc8(payload, 3U)};

    parser.expectFrame(payload, 3U);

    for (uint8_t byte : frame)
    {
        parser.pushByte(byte);
    }

    return expectPayload(parser, 3U) &&
           expect(parser.callbackCount() == 1U, "callback count should be 1") &&
           expect(parser.validFrameCount() == 1U, "valid frame count should be 1") &&
           expect(parser.crcFailCount() == 0U, "crc fail count should be 0") &&
           expect(parser.overflowCount() == 0U, "overflow count should be 0");
}

bool testPartialFrameWaitsForCrc()
{
    RecordingParser parser;
    const uint8_t payload[] = {0x33U, 0x44U};
    const uint8_t partialFrame[] = {0xAAU, 0x55U, 0x02U, 0x33U, 0x44U};

    parser.expectFrame(payload, 2U);

    for (uint8_t byte : partialFrame)
    {
        parser.pushByte(byte);
    }

    if (!expect(!parser.frameReceived(), "frame callback should not run before crc"))
    {
        return false;
    }

    if (!expect(parser.state() == OnlineParser::State::Crc, "parser should wait for crc"))
    {
        return false;
    }

    parser.pushByte(crc8(payload, 2U));

    return expectPayload(parser, 2U) &&
           expect(parser.callbackCount() == 1U, "callback count should be 1 after crc") &&
           expect(parser.validFrameCount() == 1U, "valid frame count should be 1 after crc");
}

bool testCrcFailureResyncsToNextFrame()
{
    RecordingParser parser;
    const uint8_t goodPayload[] = {0xABU, 0xCDU};
    const uint8_t stream[] = {
        0xAAU, 0x55U, 0x01U, 0x99U, 0x00U,
        0xAAU, 0x55U, 0x02U, 0xABU, 0xCDU, crc8(goodPayload, 2U)
    };

    parser.expectFrame(goodPayload, 2U);

    for (uint8_t byte : stream)
    {
        parser.pushByte(byte);
    }

    return expectPayload(parser, 2U) &&
           expect(parser.callbackCount() == 1U, "only one callback should be emitted") &&
           expect(parser.validFrameCount() == 1U, "only the valid frame should count") &&
           expect(parser.crcFailCount() == 1U, "crc fail count should be 1");
}

bool testInvalidLengthResyncsToNextFrame()
{
    RecordingParser parser;
    const uint8_t goodPayload[] = {0x42U};
    const uint8_t stream[] = {
        0xAAU, 0x55U, 0x40U,
        0xAAU, 0x55U, 0x01U, 0x42U, crc8(goodPayload, 1U)
    };

    parser.expectFrame(goodPayload, 1U);

    for (uint8_t byte : stream)
    {
        parser.pushByte(byte);
    }

    return expectPayload(parser, 1U) &&
           expect(parser.callbackCount() == 1U, "only one callback should be emitted after overflow") &&
           expect(parser.validFrameCount() == 1U, "valid frame count should be 1 after overflow") &&
           expect(parser.overflowCount() == 1U, "overflow count should be 1");
}

bool testComLibForwardsSendAndRead()
{
    MockTransport transport;
    ComLib comlib(transport);
    const uint8_t txData[] = {0x11U, 0x22U, 0x33U};
    uint8_t rxData[4] = {0U, 0U, 0U, 0U};

    if (!expect(comlib.Write(txData, sizeof(txData)), "ComLib write should succeed"))
    {
        return false;
    }

    if (!expect(transport.writeCalled, "transport write should be called"))
    {
        return false;
    }

    if (!expect(transport.lastWriteBuffer == txData, "write buffer pointer should be forwarded without copy"))
    {
        return false;
    }

    if (!expect(transport.lastWriteLength == sizeof(txData), "write length should be forwarded"))
    {
        return false;
    }

    if (!expect(comlib.Read(rxData, sizeof(rxData)) == sizeof(rxData), "ComLib read should return byte count"))
    {
        return false;
    }

    return expect(transport.readCalled, "transport read should be called") &&
           expect(transport.lastReadBuffer == rxData, "read buffer pointer should be forwarded without copy") &&
           expect(transport.lastReadLength == sizeof(rxData), "read length should be forwarded") &&
           expect(rxData[0] == 0xA0U && rxData[3] == 0xA3U, "read data should be written into caller buffer");
}
}

int main()
{
    bool ok = true;

    ok = testValidFrame() && ok;
    ok = testPartialFrameWaitsForCrc() && ok;
    ok = testCrcFailureResyncsToNextFrame() && ok;
    ok = testInvalidLengthResyncsToNextFrame() && ok;
    ok = testComLibForwardsSendAndRead() && ok;

    if (!ok)
    {
        return 1;
    }

    std::puts("All online parser tests passed.");
    return 0;
}
