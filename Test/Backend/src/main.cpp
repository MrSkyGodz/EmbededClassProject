#include "api/ApiServer.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    unsigned port = 8080U;
    if (argc >= 2)
    {
        port = static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10));
    }

    api::ApiServer server(port);
    std::string error;
    if (!server.run(error))
    {
        std::cerr << "Backend failed: " << error << '\n';
        return 1;
    }

    return 0;
}
