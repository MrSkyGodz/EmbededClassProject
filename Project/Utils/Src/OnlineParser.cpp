#include "OnlineParser.hpp"

OnlineParser::OnlineParser(size_t headerLength, size_t maxFramePayloadLength, size_t rxBufferCapacity)
    : rxBuffer_(rxBufferCapacity),
      payloadBuffer_{0U},
      state_(State::Header),
      parseOffset_(0U),
      headerLength_(headerLength),
      maxFramePayloadLength_(maxFramePayloadLength),
      matchedHeaderLength_(0U),
      payloadLength_(0U),
      payloadIndex_(0U),
      validFrameCount_(0U),
      crcFailCount_(0U),
      overflowCount_(0U),
      droppedByteCount_(0U)
{
    if ((headerLength_ == 0U) || (headerLength_ > DefaultMaxHeaderLength))
    {
        headerLength_ = DefaultMaxHeaderLength;
    }

    if ((maxFramePayloadLength_ == 0U) || (maxFramePayloadLength_ > DefaultMaxPayloadLength))
    {
        maxFramePayloadLength_ = DefaultMaxPayloadLength;
    }
}

void OnlineParser::pushByte(uint8_t byte)
{
    if (!rxBuffer_.push(byte))
    {
        droppedByteCount_++;
        return;
    }

    runStateMachine();
}

void OnlineParser::reset()
{
    rxBuffer_.clear();
    resetStateMachine();
}

OnlineParser::State OnlineParser::state() const
{
    return state_;
}

uint32_t OnlineParser::validFrameCount() const
{
    return validFrameCount_;
}

uint32_t OnlineParser::crcFailCount() const
{
    return crcFailCount_;
}

uint32_t OnlineParser::overflowCount() const
{
    return overflowCount_;
}

uint32_t OnlineParser::droppedByteCount() const
{
    return droppedByteCount_;
}

void OnlineParser::onFrameReceived(const uint8_t *payload, uint8_t payloadLength)
{
    (void) payload;
    (void) payloadLength;
}

void OnlineParser::runStateMachine()
{
    bool keepRunning = true;
    uint8_t currentByte = 0U;

    while (keepRunning)
    {
        keepRunning = false;

        switch (state_)
        {
        case State::Header:
            if (!rxBuffer_.peek(parseOffset_, currentByte))
            {
                return;
            }

            keepRunning = true;
            if (!checkHeader(matchedHeaderLength_, currentByte))
            {
                resync();
                break;
            }

            matchedHeaderLength_++;
            parseOffset_++;

            if (matchedHeaderLength_ > headerLength_)
            {
                overflowCount_++;
                resync();
                break;
            }

            if (matchedHeaderLength_ < headerLength_)
            {
                break;
            }

            state_ = State::Length;
            break;

        case State::Length:
            if (!rxBuffer_.peek(parseOffset_, currentByte))
            {
                return;
            }

            keepRunning = true;
            payloadLength_ = currentByte;
            parseOffset_++;
            payloadIndex_ = 0U;

            if (payloadLength_ > maxFramePayloadLength_)
            {
                overflowCount_++;
                resync();
                break;
            }

            state_ = (payloadLength_ == 0U) ? State::Crc : State::Payload;
            break;

        case State::Payload:
            if (payloadIndex_ >= payloadLength_)
            {
                state_ = State::Crc;
                keepRunning = true;
                break;
            }

            if (!rxBuffer_.peek(parseOffset_, currentByte))
            {
                return;
            }

            keepRunning = true;
            payloadBuffer_[payloadIndex_] = currentByte;
            payloadIndex_++;
            parseOffset_++;

            if (payloadIndex_ >= payloadLength_)
            {
                state_ = State::Crc;
            }
            break;

        case State::Crc:
            if (!rxBuffer_.peek(parseOffset_, currentByte))
            {
                return;
            }

            keepRunning = true;
            if (checkCrc(payloadBuffer_, payloadLength_, currentByte))
            {
                completeFrame();
            }
            else
            {
                crcFailCount_++;
                resync();
            }
            break;

        default:
            resetStateMachine();
            return;
        }
    }
}

void OnlineParser::resetStateMachine()
{
    state_ = State::Header;
    parseOffset_ = 0U;
    matchedHeaderLength_ = 0U;
    payloadLength_ = 0U;
    payloadIndex_ = 0U;
}

void OnlineParser::completeFrame()
{
    onFrameReceived(payloadBuffer_, payloadLength_);
    validFrameCount_++;
    rxBuffer_.discard(parseOffset_ + 1U);
    resetStateMachine();
}

void OnlineParser::resync()
{
    /* Shift the candidate start by one byte and replay the remaining data
       from the new beginning through the state machine. */
    rxBuffer_.discard(1U);
    resetStateMachine();
}
