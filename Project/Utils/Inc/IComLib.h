#pragma once

#include <stdint.h>
#include <stddef.h>

class IComLib
{
public:
	virtual ~IComLib() = default;

	virtual bool Write(const uint8_t* buffer, size_t length) = 0;

	virtual size_t Read(uint8_t* buffer, size_t bufferLength) = 0;
};



