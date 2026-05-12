#pragma once

#include "IComLib.h"

class ComLib
{
public:
	explicit ComLib(IComLib& transport);

	bool Write(const uint8_t* buffer, size_t length);
	size_t Read(uint8_t* buffer, size_t bufferLength);

private:
	IComLib& transport_;
};
