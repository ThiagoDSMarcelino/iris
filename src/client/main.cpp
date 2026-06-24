#include "client.h"
#include "log.h"

int main(int argc, char *argv[])
{
    if (argc < 4 || argc > 5)
    {
        PRINT_ERR("Usage: client <ip> <port> <filename> [output_dir]");
        return 1;
    }

    const char *outputDir = argc == 5 ? argv[4] : nullptr;
    auto clientResult = Client::create(argv[1], static_cast<uint16_t>(std::stoi(argv[2])), outputDir);
    if (!clientResult)
    {
        PRINT_ERR("Failed to create client: " << to_string(clientResult.error()));
        return 1;
    }

    auto client = std::move(clientResult.value());

    auto error = client->fetch(argv[3]);
    if (error)
    {
        PRINT_ERR("Failed to fetch file: " << to_string(*error));
        return 1;
    }

    return 0;
}
