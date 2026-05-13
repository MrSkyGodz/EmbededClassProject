#include "ComLib.h"

ComLib::ComLib(IComLib& transport)
	: transport_(transport)
{
}

bool ComLib::Write(const uint8_t* buffer, size_t length)
{
	return transport_.Write(buffer, length);
}

size_t ComLib::Read(uint8_t* buffer, size_t bufferLength)
{
	return transport_.Read(buffer, bufferLength);
}
