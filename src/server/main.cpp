#include <algorithm>
#include <cstdlib>
#include <string_view>

#include "server/fault.h"
#include "log.h"
#include "server.h"

static float parse_rate(std::string_view val)
{
    float f = std::stof(std::string(val));
    return std::clamp(f, 0.0f, 1.0f);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        PRINT_ERR("Usage: server <port> <search_directory> [options]");
        PRINT_ERR("  --fault-drop=<rate>      Probability [0,1] to silently drop a packet");
        PRINT_ERR("  --fault-seq=<rate>       Probability to corrupt the seq field");
        PRINT_ERR("  --fault-checksum=<rate>  Probability to corrupt the checksum field");
        PRINT_ERR("  --fault-payload=<rate>   Probability to flip a random payload byte");
        return 1;
    }

    FaultConfig faultConfig;

    for (int i = 3; i < argc; i++)
    {
        std::string_view arg(argv[i]);
        auto value = arg.substr(arg.find('=') + 1);

        if (arg.starts_with("--fault-drop="))
        {
            faultConfig.dropRate = parse_rate(value);
        }
        else if (arg.starts_with("--fault-seq="))
        {
            faultConfig.corruptSeqRate = parse_rate(value);
        }
        else if (arg.starts_with("--fault-checksum="))
        {
            faultConfig.corruptChecksumRate = parse_rate(value);
        }
        else if (arg.starts_with("--fault-payload="))
        {
            faultConfig.corruptPayloadRate = parse_rate(value);
        }
        else
        {
            PRINT_ERR("Unknown option: " << arg);
            return 1;
        }
    }

    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));

    auto serverResult = Server::create(port, argv[2], faultConfig);
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
