#pragma once

#include "transport/ITransport.h"

#include <memory>
#include <string>
#include <vector>

namespace transport
{
struct TransportFactoryResult
{
    std::string mode;
    std::unique_ptr<ITransport> transport;
};

TransportFactoryResult createTransport(const std::string& requestedMode);
std::vector<std::string> supportedTransportModes();
}
