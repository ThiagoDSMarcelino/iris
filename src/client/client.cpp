#include "client.h"

#include <fstream>
#include <cstring>

#include "log.h"
#include "protocol.h"

namespace fs = std::filesystem;

constexpr uint32_t TIMEOUT_MS = 30000;

std::string Client::resolve_output_path(const char *filename)
{
    fs::path base(filename);

    fs::path stem = base.stem();
    fs::path ext = base.extension();
    fs::path dir = this->outputDir.has_value() ? this->outputDir.value() : base.parent_path();

    fs::path candidate = dir / (stem.string() + ext.string());

    for (int i = 1; fs::exists(candidate); i++)
    {
        candidate = dir / (stem.string() + " (" + std::to_string(i) + ")" + ext.string());
    }

    return candidate.string();
}

const char *to_string(ClientError error)
{
    switch (error)
    {
    case ClientError::SocketCreationFailed:
        return "socket creation failed";
    case ClientError::SendFailed:
        return "send failed";
    case ClientError::ReceiveFailed:
        return "receive failed";
    case ClientError::DeserializeFailed:
        return "deserialize failed";
    case ClientError::OutputFileFailed:
        return "output file failed";
    case ClientError::InvalidOutputDirectory:
        return "invalid output directory";
    case ClientError::Timeout:
        return "timeout";
    }

    return "unknown error";
}

std::expected<std::unique_ptr<Client>, ClientError> Client::create(const char *ip, uint16_t port, const char *outputDir)
{
    auto socketResult = luft::network::Socket::create();
    if (!socketResult)
    {
        return std::unexpected(ClientError::SocketCreationFailed);
    }

    if (outputDir && (!fs::exists(outputDir) || !fs::is_directory(outputDir)))
    {
        return std::unexpected(ClientError::InvalidOutputDirectory);
    }

    auto client = std::unique_ptr<Client>(new Client());
    client->socket = std::move(socketResult.value());
    client->ip = ip;
    client->port = port;
    client->outputDir = outputDir ? std::optional<fs::path>(fs::path(outputDir)) : std::nullopt;
    return client;
}

std::optional<ClientError> Client::fetch(const char *filename)
{
    auto filenamePtr = reinterpret_cast<const std::byte *>(filename);
    std::vector<std::byte> payload(filenamePtr, filenamePtr + std::strlen(filename));
    auto packet = luft::protocol::serialize(0, luft::protocol::Code::DATA, 1, payload);

    if (!this->socket->send(this->ip, this->port, packet.data(), packet.size()))
    {
        return ClientError::SendFailed;
    }

    std::string outputPath = this->resolve_output_path(filename);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        return ClientError::OutputFileFailed;
    }

    auto cleanup = [&](ClientError err) -> ClientError
    {
        output.close();
        fs::remove(outputPath);
        return err;
    };

    uint32_t packetsCount = 0;
    uint8_t seq = 0;
    while (true)
    {
        auto message = this->socket->receive_with_timeout(TIMEOUT_MS);
        if (!message)
        {
            if (message.error() == luft::network::ReceiveError::Timeout)
            {
                return cleanup(ClientError::Timeout);
            }

            LOG_ERR("Failed to receive message");
            const auto ackPacket = luft::protocol::serialize(seq, luft::protocol::Code::NACK, 0, {});
            this->socket->send(this->ip, this->port, ackPacket.data(), ackPacket.size());
            continue;
        }

        auto packetResult = luft::protocol::deserialize(message->data, message->size);
        if (!packetResult)
        {
            LOG_ERR("Failed to deserialize packet");
            const auto ackPacket = luft::protocol::serialize(seq, luft::protocol::Code::NACK, 0, {});
            this->socket->send(this->ip, this->port, ackPacket.data(), ackPacket.size());
            continue;
        }

        auto packet = packetResult.value();

        if (packet.seq != seq)
        {
            LOG_ERR("Unexpected sequence number: " << static_cast<int>(packet.seq) << ", expected: " << static_cast<int>(seq));
            const auto ackPacket = luft::protocol::serialize(packet.seq, luft::protocol::Code::NACK, 0, {});
            this->socket->send(this->ip, this->port, ackPacket.data(), ackPacket.size());
            continue;
        }

        if (packet.code == static_cast<uint8_t>(luft::protocol::Code::ERROR))
        {
            std::string errorMessage(reinterpret_cast<const char *>(packet.payload.data()), packet.payload.size());
            PRINT_ERR("Server error: " << errorMessage);
            return cleanup(ClientError::ReceiveFailed);
        }

        if (packet.code != static_cast<uint8_t>(luft::protocol::Code::DATA))
        {
            LOG_ERR("Unexpected packet code: " << static_cast<int>(packet.code));
            return cleanup(ClientError::ReceiveFailed);
        }

        output.write(reinterpret_cast<const char *>(packet.payload.data()), packet.size);

        packetsCount++;
        LOG("Received packet " << packetsCount << "/" << packet.total << " with " << packet.size << " bytes of payload");

        const auto ackPacket = luft::protocol::serialize(seq, luft::protocol::Code::ACK, 0, {});
        this->socket->send(this->ip, this->port, ackPacket.data(), ackPacket.size());
        seq = !seq;

        if (packetsCount == packet.total)
        {
            PRINT("File \"" << filename << "\" received successfully");
            break;
        }
    }

    return std::nullopt;
}