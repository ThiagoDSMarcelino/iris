#include <cstdint>
#include <cstdlib>

#include "log.h"
#include "server.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        PRINT_ERR("Usage: server <port> <search_directory>");
        return 1;
    }

    auto port = static_cast<uint16_t>(std::atoi(argv[1]));

    auto serverResult = Server::create(port, argv[2]);
    if (!serverResult)
    {
        PRINT_ERR("Failed to create server: " << to_string(serverResult.error()));
        return 1;
    }

    if (auto error = serverResult.value()->serve())
    {
        PRINT_ERR("Server error: " << to_string(error.value()));
        return 1;
    }

    return 0;
}
