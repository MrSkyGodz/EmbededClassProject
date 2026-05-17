#pragma once

#include "IcdFrameTransmitter.hpp"
#include "IcdFrameQueue.hpp"
#include "IcdMessageCodec.hpp"

class IcdUartPublisher
{
public:
	IcdUartPublisher()
	{
	}

	void Configure(const IcdFrameTransmitterConfig_t& config)
	{
		transmitter_.Configure(config);
	}

	bool Publish(const IcdMessage_t& message)
	{
		uint8_t frame[IcdFrameQueue::MaxBytes] = {0U};
		const uint8_t frameLength = IcdMessageCodec::BuildFrame(message, frame);
		if (frameLength == 0U)
		{
			return false;
		}

		return PushFrame(frame, frameLength);
	}

	void Process()
	{
		if (!transmitter_.CanStartNext() || frameQueue_.Empty())
		{
			return;
		}
		if (transmitter_.Start(frameQueue_.FrontData(), frameQueue_.FrontLength()))
		{
			frameQueue_.Pop();
		}
	}

private:
	bool PushFrame(const uint8_t* frame, uint8_t frameLength)
	{
		return frameQueue_.Push(frame, frameLength);
	}

	IcdFrameQueue frameQueue_;
	IcdFrameTransmitter transmitter_;
};
