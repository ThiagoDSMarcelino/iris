#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>

#include "socket.h"
#include "protocol.h"
#include "server/fault.h"
#include "server/session.h"

constexpr size_t PAYLOAD_SIZE = luft::network::BUFFER_SIZE - luft::protocol::HEADER_SIZE;

enum class ServerError
{
    InvalidDirectory,
    SocketCreationFailed,
    BindFailed,
};

const char *to_string(ServerError error);

class Server : public std::enable_shared_from_this<Server>
{
public:
    ~Server() = default;

    static std::expected<std::shared_ptr<Server>, ServerError> create(uint16_t port, const char *searchDir, FaultConfig faultConfig = {});

    std::optional<ServerError> serve();

private:
    Server() = default;

    void handle_session(std::shared_ptr<Session> session, luft::protocol::Packet packet);
    void send_packet(const char *ip, uint16_t port, std::vector<std::byte> packet);

    std::unique_ptr<luft::network::Socket> socket;
    std::filesystem::path searchDir;
    uint16_t port;
    FaultInjector faultInjector{FaultConfig{}};

    SessionManager sessionManager;
};
