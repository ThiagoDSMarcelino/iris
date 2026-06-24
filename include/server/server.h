#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "chat.h"
#include "socket.h"

enum class ServerError
{
    InvalidDirectory,
    SocketCreationFailed,
    BindFailed,
    ListenFailed,
};

const char *to_string(ServerError error);

class Server : public std::enable_shared_from_this<Server>
{
public:
    ~Server() = default;

    static std::expected<std::shared_ptr<Server>, ServerError> create(uint16_t port, const char *searchDir);

    std::optional<ServerError> serve();

private:
    Server() = default;

    void handle_connection(std::unique_ptr<iris::network::Socket> conn);
    void handle_file_request(iris::network::Socket *conn, const std::string &peer);
    void handle_chat(std::shared_ptr<iris::network::Socket> conn, const std::string &peer);

    std::unique_ptr<iris::network::Socket> socket;
    std::filesystem::path searchDir;
    iris::chat::ChatHub chatHub;
    uint16_t port;
};
