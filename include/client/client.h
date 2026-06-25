#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>

#include "socket.h"

enum class ClientError
{
    SocketCreationFailed,
    ConnectFailed,
    SendFailed,
    ReceiveFailed,
    OutputFileFailed,
    InvalidOutputDirectory,
    ChecksumMismatch,
};

const char *to_string(ClientError error);

class Client
{
public:
    ~Client() = default;

    static std::expected<std::unique_ptr<Client>, ClientError> create(const char *ip, uint16_t port);

    std::optional<ClientError> fetch(const char *filename, const char *outputDir = nullptr);
    std::optional<ClientError> chat(const std::string &nick, const std::string &room);

private:
    Client() = default;

    std::unique_ptr<iris::network::Socket> socket;
    const char *ip;
    uint16_t port;
};
