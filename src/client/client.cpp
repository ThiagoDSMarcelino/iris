#include "client.h"

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include "log.h"
#include "protocol.h"
#include "sha256.h"

namespace fs = std::filesystem;

static std::optional<std::string> receive_chat_line(iris::network::Socket *sock)
{
    std::byte lengthBuf[iris::protocol::LENGTH_PREFIX_SIZE];
    if (!sock->receive_all(lengthBuf, sizeof(lengthBuf)))
    {
        return std::nullopt;
    }

    auto length = iris::protocol::parse_u16_length(lengthBuf, sizeof(lengthBuf));
    if (!length)
    {
        return std::nullopt;
    }

    std::string line(*length, '\0');
    if (*length > 0 && !sock->receive_all(line.data(), *length))
    {
        return std::nullopt;
    }

    return line;
}

static std::string resolve_output_path(const char *filename, const std::optional<fs::path> &outputDir)
{
    fs::path base(filename);

    fs::path stem = base.stem();
    fs::path ext = base.extension();
    fs::path dir = outputDir.has_value() ? outputDir.value() : base.parent_path();

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
    case ClientError::ConnectFailed:
        return "connect failed";
    case ClientError::SendFailed:
        return "send failed";
    case ClientError::ReceiveFailed:
        return "receive failed";
    case ClientError::OutputFileFailed:
        return "output file failed";
    case ClientError::InvalidOutputDirectory:
        return "invalid output directory";
    case ClientError::ChecksumMismatch:
        return "checksum mismatch";
    }

    return "unknown error";
}

std::expected<std::unique_ptr<Client>, ClientError> Client::create(const char *ip, uint16_t port)
{
    auto socketResult = iris::network::Socket::create();
    if (!socketResult)
    {
        return std::unexpected(ClientError::SocketCreationFailed);
    }

    auto client = std::unique_ptr<Client>(new Client());
    client->socket = std::move(socketResult.value());
    client->ip = ip;
    client->port = port;
    return client;
}

std::optional<ClientError> Client::fetch(const char *filename, const char *outputDir)
{
    if (outputDir && (!fs::exists(outputDir) || !fs::is_directory(outputDir)))
    {
        return ClientError::InvalidOutputDirectory;
    }

    if (!this->socket->connect(this->ip, this->port))
    {
        return ClientError::ConnectFailed;
    }

    auto request = iris::protocol::serialize_file_request(filename);
    if (!this->socket->send_all(request.data(), request.size()))
    {
        return ClientError::SendFailed;
    }

    std::byte headerBuf[iris::protocol::RESPONSE_HEADER_SIZE];
    if (!this->socket->receive_all(headerBuf, sizeof(headerBuf)))
    {
        return ClientError::ReceiveFailed;
    }

    auto header = iris::protocol::parse_response_header(headerBuf, sizeof(headerBuf));
    if (!header)
    {
        return ClientError::ReceiveFailed;
    }

    if (header->status != iris::protocol::Status::OK)
    {
        std::string message(header->length, '\0');
        if (header->length > 0 && !this->socket->receive_all(message.data(), header->length))
        {
            return ClientError::ReceiveFailed;
        }
        PRINT_ERR("Server error: " << message);
        return ClientError::ReceiveFailed;
    }

    std::optional<fs::path> outputDirPath = outputDir ? std::optional<fs::path>(fs::path(outputDir)) : std::nullopt;
    std::string outputPath = resolve_output_path(filename, outputDirPath);

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

    iris::crypto::Sha256 hasher;
    std::vector<std::byte> buffer(iris::network::BUFFER_SIZE);
    uint64_t received = 0;

    while (received < header->length)
    {
        auto want = static_cast<size_t>(std::min<uint64_t>(buffer.size(), header->length - received));

        auto got = this->socket->receive(buffer.data(), want);
        if (!got || *got == 0)
        {
            return cleanup(ClientError::ReceiveFailed);
        }

        hasher.update(buffer.data(), *got);

        output.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(*got));
        if (!output)
        {
            return cleanup(ClientError::OutputFileFailed);
        }

        received += *got;
        LOG("Received " << received << "/" << header->length << " bytes");
    }

    iris::crypto::Sha256Digest expected{};
    if (!this->socket->receive_all(expected.data(), expected.size()))
    {
        return cleanup(ClientError::ReceiveFailed);
    }

    auto actual = hasher.finalize();

    PRINT("Expected checksum: " << iris::crypto::to_hex(expected));
    PRINT("Actual checksum:   " << iris::crypto::to_hex(actual));

    if (actual != expected)
    {
        PRINT_ERR("Checksum mismatch");
        return cleanup(ClientError::ChecksumMismatch);
    }

    PRINT("File \"" << filename << "\" received successfully, saved to \"" << outputPath << "\"");
    return std::nullopt;
}

std::optional<ClientError> Client::chat(const std::string &nick, const std::string &room)
{
    if (!this->socket->connect(this->ip, this->port))
    {
        return ClientError::ConnectFailed;
    }

    auto join = iris::protocol::serialize_chat_join(nick, room);
    if (!this->socket->send_all(join.data(), join.size()))
    {
        return ClientError::SendFailed;
    }

    PRINT("Conectado à sala \"" << room << "\" como \"" << nick << "\". Digite mensagens (/sair para encerrar).");

    iris::network::Socket *sock = this->socket.get();

    struct pollfd fds[2] = {};
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sock->native_handle();
    fds[1].events = POLLIN;

    std::string stdinBuffer;
    bool running = true;

    while (running)
    {
        if (::poll(fds, 2, -1) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR))
        {
            auto line = receive_chat_line(sock);
            if (!line)
            {
                PRINT_ERR("Conexão com o servidor encerrada.");
                break;
            }
            PRINT(*line);
        }

        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            char buffer[4096];
            ssize_t got = ::read(STDIN_FILENO, buffer, sizeof(buffer));
            if (got <= 0)
            {
                break;
            }
            stdinBuffer.append(buffer, static_cast<size_t>(got));

            size_t newline;
            while (running && (newline = stdinBuffer.find('\n')) != std::string::npos)
            {
                std::string message = stdinBuffer.substr(0, newline);
                stdinBuffer.erase(0, newline + 1);
                if (!message.empty() && message.back() == '\r')
                {
                    message.pop_back();
                }

                if (message == "/sair")
                {
                    running = false;
                    break;
                }
                if (message.empty())
                {
                    continue;
                }
                if (message.size() > std::numeric_limits<uint16_t>::max())
                {
                    PRINT_ERR("Mensagem muito longa (máximo " << std::numeric_limits<uint16_t>::max() << " bytes).");
                    continue;
                }

                auto frame = iris::protocol::serialize_chat_message(message);
                if (!this->socket->send_all(frame.data(), frame.size()))
                {
                    running = false;
                    break;
                }
            }
        }
    }

    return std::nullopt;
}
