#ifndef ONLINE_PARSER_HPP
#define ONLINE_PARSER_HPP

#include <stddef.h>
#include <stdint.h>

#include "CircularBuffer.hpp"

class OnlineParser
{
public:
    enum class State
    {
        Header = 0,
        Length,
        Payload,
        Crc
    };

    static constexpr size_t DefaultMaxHeaderLength = 16U;
    static constexpr size_t DefaultMaxPayloadLength = 128U;

    OnlineParser(size_t headerLength = DefaultMaxHeaderLength,
                 size_t maxFramePayloadLength = DefaultMaxPayloadLength,
                 size_t rxBufferCapacity = CircularBuffer::MaxCapacity);
    virtual ~OnlineParser() = default;

    void pushByte(uint8_t byte);
    void reset();

    State state() const;
    uint32_t validFrameCount() const;
    uint32_t crcFailCount() const;
    uint32_t overflowCount() const;
    uint32_t droppedByteCount() const;

protected:
    virtual bool checkHeader(size_t headerIndex, uint8_t byte) const = 0;
    virtual bool checkCrc(const uint8_t *payload, uint8_t payloadLength, uint8_t crcByte) const = 0;
    // The payload pointer is valid only for the duration of this callback.
    virtual void onFrameReceived(const uint8_t *payload, uint8_t payloadLength);

private:
    void runStateMachine();
    void resetStateMachine();
    void completeFrame();
    void resync();

    CircularBuffer rxBuffer_;
    
    uint8_t payloadBuffer_[DefaultMaxPayloadLength];

    State state_;
    
    size_t parseOffset_;
    size_t headerLength_;

    size_t maxFramePayloadLength_;
    size_t matchedHeaderLength_;

    uint8_t payloadLength_;
    uint8_t payloadIndex_;

    uint32_t validFrameCount_;
    uint32_t crcFailCount_;
    uint32_t overflowCount_;
    uint32_t droppedByteCount_;
};

#endif
