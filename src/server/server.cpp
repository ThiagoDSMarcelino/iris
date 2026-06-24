#include "server.h"

#include <chrono>
#include <cstring>
#include <thread>

#include "log.h"
#include "server/chunk.h"
#include "server/rtt.h"
#include "protocol.h"

constexpr uint8_t MAX_TIMEOUTS = 5;

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
    }

    return "unknown error";
}

std::expected<std::shared_ptr<Server>, ServerError> Server::create(uint16_t port, const char *searchDir, FaultConfig faultConfig)
{
    if (!std::filesystem::is_directory(searchDir))
    {
        return std::unexpected(ServerError::InvalidDirectory);
    }

    auto socketResult = luft::network::Socket::create();
    if (!socketResult.has_value())
    {
        return std::unexpected(ServerError::SocketCreationFailed);
    }

    auto server = std::shared_ptr<Server>(new Server());
    server->socket = std::move(socketResult.value());
    server->searchDir = std::filesystem::path(searchDir);
    server->port = port;
    server->faultInjector = FaultInjector(faultConfig);

    return server;
}

std::optional<ServerError> Server::serve()
{
    if (!this->socket->bind(this->port))
    {
        LOG_ERR("Failed to bind on port " << this->port);
        return ServerError::BindFailed;
    }

    LOG("Server listening on port " << this->port);

    while (true)
    {
        auto message = this->socket->receive();

        if (!message)
        {
            LOG_ERR("Failed to receive");
            continue;
        }

        std::string ip = message->senderIp;
        uint16_t port = message->senderPort;

        auto session = sessionManager.find(ip, port);
        if (session)
        {
            std::lock_guard qLock(session->mutex);
            session->queue.push(*message);
            session->cv.notify_one();
        }
        else
        {
            auto packetResult = luft::protocol::deserialize(message->data, message->size);
            if (!packetResult)
            {
                continue;
            }

            if (packetResult->code != static_cast<uint8_t>(luft::protocol::Code::DATA))
            {
                continue;
            }

            auto newSession = sessionManager.create(ip, port);

            LOG("New session for " << ip << ":" << port);

            auto self = shared_from_this();
            std::thread([self, newSession, packet = std::move(*packetResult), ip, port]()
                        {
                self->handle_session(newSession, std::move(packet));
                self->sessionManager.remove(ip, port);
                LOG("Session closed for " << ip << ":" << port); })
                .detach();
        }
    }

    return std::nullopt;
}

void Server::send_packet(const char *ip, uint16_t port, std::vector<std::byte> packet)
{
    auto result = this->faultInjector.inject(std::move(packet));
    if (!result)
    {
        return;
    }

    this->socket->send(ip, port, result->data(), result->size());
}

static std::vector<std::byte> error_packet(const char *message)
{
    auto messagePtr = reinterpret_cast<const std::byte *>(message);
    std::vector<std::byte> payload(messagePtr, messagePtr + std::strlen(message));

    auto packet = luft::protocol::serialize(0, luft::protocol::Code::ERROR, 1, payload);
    return packet;
}

void Server::handle_session(std::shared_ptr<Session> session, luft::protocol::Packet packet)
{
    std::string_view filename(reinterpret_cast<const char *>(packet.payload.data()), packet.payload.size());

    auto path = searchDir / filename;

    bool exists = std::filesystem::exists(path);

    if (!exists)
    {
        auto errPacket = error_packet("File not found");
        send_packet(session->clientIp.c_str(), session->clientPort, errPacket);
        return;
    }

    auto chunkedResult = chunk_file(path, PAYLOAD_SIZE);

    if (!chunkedResult.has_value())
    {
        LOG_ERR("Failed to chunk file: " << to_string(chunkedResult.error()));
        return;
    }

    uint8_t seq = 0;
    RttEstimator rtt;

    for (const auto &chunk : chunkedResult->chunks)
    {
        auto dataPacket = luft::protocol::serialize(seq, luft::protocol::Code::DATA, chunkedResult->total, chunk.payload);

        auto sendTime = std::chrono::steady_clock::now();
        send_packet(session->clientIp.c_str(), session->clientPort, dataPacket);

        uint8_t timeouts = 0;

    wait_for_ack:
        luft::network::Message ackMsg;
        {
            std::unique_lock lock(session->mutex);
            bool received = session->cv.wait_for(
                lock,
                std::chrono::milliseconds(rtt.timeout_ms()),
                [&session]
                { return !session->queue.empty(); });

            if (!received)
            {
                if (++timeouts > MAX_TIMEOUTS)
                {
                    LOG_ERR("Max timeouts reached, aborting transfer");
                    return;
                }
                LOG_ERR("Timeout waiting for ACK, retransmitting (" << (int)timeouts << "/" << (int)MAX_TIMEOUTS << ")");
                sendTime = std::chrono::steady_clock::now();
                send_packet(session->clientIp.c_str(), session->clientPort, dataPacket);
                goto wait_for_ack;
            }

            ackMsg = session->queue.front();
            session->queue.pop();
        }

        auto ackPacketResult = luft::protocol::deserialize(ackMsg.data, ackMsg.size);

        if (!ackPacketResult)
        {
            LOG_ERR("Failed to deserialize ACK, retransmitting");
            sendTime = std::chrono::steady_clock::now();
            send_packet(session->clientIp.c_str(), session->clientPort, dataPacket);
            goto wait_for_ack;
        }

        // Reset timeouts on successful ACK reception
        // Client responses independent if is a NACK or ACK, since both are valid responses to the sent packet
        timeouts = 0;

        auto ackPacket = ackPacketResult.value();

        if (ackPacket.code != static_cast<uint8_t>(luft::protocol::Code::ACK) || ackPacket.seq != seq)
        {
            sendTime = std::chrono::steady_clock::now();
            send_packet(session->clientIp.c_str(), session->clientPort, dataPacket);
            goto wait_for_ack;
        }

        {
            auto sampleRtt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sendTime).count();
            rtt.update(sampleRtt);
        }

        seq = !seq;
    }

    return;
}
