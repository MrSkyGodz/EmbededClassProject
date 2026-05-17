#pragma once

#include <stdint.h>

typedef bool (*IcdFrameWriteFn)(void* context, const uint8_t* frame, uint8_t frameLength);
typedef bool (*IcdFrameBusyFn)(void* context);

typedef struct
{
	IcdFrameWriteFn Write;
	IcdFrameBusyFn IsBusy;
	void* Context;
} IcdFrameTransmitterConfig_t;

class IcdFrameTransmitter
{
public:
	IcdFrameTransmitter()
		: config_{nullptr, nullptr, nullptr},
		  txInProgress_(0U)
	{
	}

	void Configure(const IcdFrameTransmitterConfig_t& config)
	{
		config_ = config;
		txInProgress_ = 0U;
	}

	bool Configured() const
	{
		return config_.Write != nullptr;
	}

	bool CanStartNext()
	{
		if (txInProgress_ == 0U)
		{
			return true;
		}

		if ((config_.IsBusy != nullptr) && config_.IsBusy(config_.Context))
		{
			return false;
		}

		txInProgress_ = 0U;
		return true;
	}

	bool Start(const uint8_t* frame, uint8_t frameLength)
	{
		if (!Configured() || (frame == nullptr) || (frameLength == 0U) || (txInProgress_ != 0U))
		{
			return false;
		}

		if (!config_.Write(config_.Context, frame, frameLength))
		{
			return false;
		}

		txInProgress_ = 1U;
		return true;
	}

private:
	IcdFrameTransmitterConfig_t config_;
	uint8_t txInProgress_;
};
