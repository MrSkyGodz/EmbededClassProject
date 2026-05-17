#pragma once

#include <stdint.h>

template<uint8_t MaxItemsValue, uint8_t MaxBytesValue>
class FixedFrameQueue
{
public:
	static constexpr uint8_t MaxItems = MaxItemsValue;
	static constexpr uint8_t MaxBytes = MaxBytesValue;

	FixedFrameQueue()
		: frames_{},
		  lengths_{},
		  head_(0U),
		  tail_(0U),
		  count_(0U)
	{
	}

	bool Push(const uint8_t* frame, uint8_t frameLength)
	{
		if ((frame == nullptr) || (frameLength == 0U) || (frameLength > MaxBytes) || (count_ >= MaxItems))
		{
			return false;
		}

		for (uint8_t i = 0U; i < frameLength; ++i)
		{
			frames_[head_][i] = frame[i];
		}
		lengths_[head_] = frameLength;
		head_ = NextIndex(head_);
		count_++;
		return true;
	}

	const uint8_t* FrontData() const
	{
		return Empty() ? nullptr : frames_[tail_];
	}

	uint8_t FrontLength() const
	{
		return Empty() ? 0U : lengths_[tail_];
	}

	void Pop()
	{
		if (!Empty())
		{
			tail_ = NextIndex(tail_);
			count_--;
		}
	}

	bool Empty() const
	{
		return count_ == 0U;
	}

	uint8_t Count() const
	{
		return count_;
	}

private:
	static uint8_t NextIndex(uint8_t index)
	{
		return static_cast<uint8_t>((index + 1U) % MaxItems);
	}

	uint8_t frames_[MaxItems][MaxBytes];
	uint8_t lengths_[MaxItems];
	uint8_t head_;
	uint8_t tail_;
	uint8_t count_;
};
