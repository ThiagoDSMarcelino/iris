#include "server.h"

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "log.h"
#include "protocol.h"

const char *to_string(ServerError error)
{
    switch (error)
    {
    case ServerError::InvalidDirectory:
        return "invalid directory";

    case ServerError::SocketCreationFailed:
        return "socket creation failed";

    case ServerError::BindFailed:
        return "bind failed";

    case ServerError::ListenFailed:
        return "listen failed";
    }

    return "unknown error";
}

std::expected<std::shared_ptr<Server>, ServerError> Server::create(uint16_t port, const char *searchDir)
{
    if (!std::filesystem::is_directory(searchDir))
    {
        return std::unexpected(ServerError::InvalidDirectory);
    }

    auto socketResult = iris::network::Socket::create();
    if (!socketResult.has_value())
    {
        return std::unexpected(ServerError::SocketCreationFailed);
    }

    auto server = std::shared_ptr<Server>(new Server());
    server->socket = std::move(socketResult.value());
    server->searchDir = std::filesystem::path(searchDir);
    server->port = port;

    return server;
}

std::optional<ServerError> Server::serve()
{
    if (!this->socket->bind(this->port))
    {
        LOG_ERR("Failed to bind on port " << this->port);
        return ServerError::BindFailed;
    }

    if (!this->socket->listen())
    {
        LOG_ERR("Failed to listen on port " << this->port);
        return ServerError::ListenFailed;
    }

    LOG("Server listening on port " << this->port);

    while (true)
    {
        auto conn = this->socket->accept();
        if (!conn)
        {
            LOG_ERR("Failed to accept connection");
            continue;
        }

        auto self = shared_from_this();
        std::thread([self, sock = std::move(*conn)]() mutable
                    { self->handle_connection(std::move(sock)); })
            .detach();
    }

    return std::nullopt;
}

static void send_status(iris::network::Socket *conn, iris::protocol::Status status, const std::string &message)
{
    auto header = iris::protocol::serialize_response_header(status, message.size());
    conn->send_all(header.data(), header.size());
    conn->send_all(message.data(), message.size());
}

void Server::handle_connection(std::unique_ptr<iris::network::Socket> conn)
{
    std::string peer = std::string(conn->peer_ip()) + ":" + std::to_string(conn->peer_port());
    LOG("New connection from " << peer);

    std::byte opcodeBuf[iris::protocol::OPCODE_SIZE];
    if (!conn->receive_all(opcodeBuf, sizeof(opcodeBuf)))
    {
        LOG_ERR("Failed to read opcode from " << peer);
        return;
    }

    auto opcode = iris::protocol::parse_opcode(opcodeBuf, sizeof(opcodeBuf));
    if (!opcode)
    {
        LOG_ERR("Invalid opcode from " << peer);
        return;
    }

    switch (*opcode)
    {
    case iris::protocol::MessageType::FileRequest:
        this->handle_file_request(conn.get(), peer);
        break;

    case iris::protocol::MessageType::ChatJoin:
        this->handle_chat(std::move(conn), peer);
        break;

    default:
        LOG_ERR("Unsupported opcode from " << peer);
        break;
    }
}

void Server::handle_file_request(iris::network::Socket *conn, const std::string &peer)
{
    std::byte lengthBuf[iris::protocol::LENGTH_PREFIX_SIZE];
    if (!conn->receive_all(lengthBuf, sizeof(lengthBuf)))
    {
        LOG_ERR("Failed to read request header from " << peer);
        return;
    }

    auto nameLength = iris::protocol::parse_u16_length(lengthBuf, sizeof(lengthBuf));
    if (!nameLength || *nameLength == 0)
    {
        LOG_ERR("Invalid request from " << peer);
        return;
    }

    std::string filename(*nameLength, '\0');
    if (!conn->receive_all(filename.data(), *nameLength))
    {
        LOG_ERR("Failed to read filename from " << peer);
        return;
    }

    auto path = this->searchDir / filename;

    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
    {
        send_status(conn, iris::protocol::Status::NOT_FOUND, "File not found");
        LOG("File not found: " << filename);
        return;
    }

    uint64_t fileSize = std::filesystem::file_size(path, ec);
    if (ec)
    {
        send_status(conn, iris::protocol::Status::ERROR, "Failed to stat file");
        LOG_ERR("Failed to stat file: " << filename);
        return;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        send_status(conn, iris::protocol::Status::ERROR, "Failed to open file");
        LOG_ERR("Failed to open file: " << filename);
        return;
    }

    auto responseHeader = iris::protocol::serialize_response_header(iris::protocol::Status::OK, fileSize);
    if (!conn->send_all(responseHeader.data(), responseHeader.size()))
    {
        LOG_ERR("Failed to send response header to " << peer);
        return;
    }

    std::vector<std::byte> buffer(iris::network::BUFFER_SIZE);
    uint64_t sent = 0;

    while (sent < fileSize)
    {
        file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        auto chunk = static_cast<size_t>(file.gcount());
        if (chunk == 0)
        {
            break;
        }

        if (!conn->send_all(buffer.data(), chunk))
        {
            LOG_ERR("Failed to send file data to " << peer);
            return;
        }
        sent += chunk;
    }

    LOG("Sent \"" << filename << "\" (" << sent << " bytes) to " << peer);
}

static std::optional<std::string> read_length_prefixed(iris::network::Socket *conn)
{
    std::byte lengthBuf[iris::protocol::LENGTH_PREFIX_SIZE];
    if (!conn->receive_all(lengthBuf, sizeof(lengthBuf)))
    {
        return std::nullopt;
    }

    auto length = iris::protocol::parse_u16_length(lengthBuf, sizeof(lengthBuf));
    if (!length)
    {
        return std::nullopt;
    }

    std::string value(*length, '\0');
    if (*length > 0 && !conn->receive_all(value.data(), *length))
    {
        return std::nullopt;
    }

    return value;
}

void Server::handle_chat(std::shared_ptr<iris::network::Socket> conn, const std::string &peer)
{
    auto nick = read_length_prefixed(conn.get());
    auto room = read_length_prefixed(conn.get());
    if (!nick || !room || nick->empty() || room->empty())
    {
        LOG_ERR("Invalid chat join from " << peer);
        return;
    }

    auto client = std::make_shared<iris::chat::ChatClient>();
    client->socket = conn;
    client->nick = *nick;

    this->chatHub.join(*room, client);
    LOG(*nick << " (" << peer << ") joined room \"" << *room << "\"");

    while (true)
    {
        std::byte opcodeBuf[iris::protocol::OPCODE_SIZE];
        if (!conn->receive_all(opcodeBuf, sizeof(opcodeBuf)))
        {
            break; // peer disconnected
        }

        auto opcode = iris::protocol::parse_opcode(opcodeBuf, sizeof(opcodeBuf));
        if (!opcode || *opcode != iris::protocol::MessageType::ChatMessage)
        {
            break;
        }

        auto text = read_length_prefixed(conn.get());
        if (!text)
        {
            break;
        }

        this->chatHub.broadcast(*room, client, *text);
    }

    this->chatHub.leave(*room, client);
    LOG(*nick << " (" << peer << ") left room \"" << *room << "\"");
}
