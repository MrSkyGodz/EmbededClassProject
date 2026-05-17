#include "transport/TransportFactory.h"

#include "transport/MockTransport.h"
#include "transport/SerialTransport.h"

namespace transport
{
TransportFactoryResult createTransport(const std::string& requestedMode)
{
    if (requestedMode == "serial")
    {
        return {"serial", std::make_unique<SerialTransport>()};
    }

    return {"mock", std::make_unique<MockTransport>()};
}

std::vector<std::string> supportedTransportModes()
{
    return {"mock", "serial"};
}
}
