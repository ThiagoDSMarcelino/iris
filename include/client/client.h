#pragma once

#include <expected>
#include <filesystem>
#include <memory>

#include "socket.h"

enum class ClientError
{
    SocketCreationFailed,
    SendFailed,
    ReceiveFailed,
    DeserializeFailed,
    OutputFileFailed,
    InvalidOutputDirectory,
    Timeout,
};

const char *to_string(ClientError error);

class Client
{
public:
    ~Client() = default;

    static std::expected<std::unique_ptr<Client>, ClientError> create(const char *ip, uint16_t port, const char *outputDir = nullptr);

    std::optional<ClientError> fetch(const char *filename);

private:
    Client() = default;

    std::string resolve_output_path(const char *filename);

    std::unique_ptr<luft::network::Socket> socket;
    const char *ip;
    uint16_t port;
    std::optional<std::filesystem::path> outputDir;
};
