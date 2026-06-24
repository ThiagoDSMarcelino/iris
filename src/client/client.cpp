#include "client.h"

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <limits>
#include <vector>

#include "log.h"
#include "protocol.h"

namespace fs = std::filesystem;

namespace
{
    // Reads one server -> client chat line frame: [u16 length][text].
    std::optional<std::string> receive_chat_line(iris::network::Socket *sock)
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
} // namespace

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
    }

    return "unknown error";
}

std::expected<std::unique_ptr<Client>, ClientError> Client::create(const char *ip, uint16_t port, const char *outputDir)
{
    auto socketResult = iris::network::Socket::create();
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

    std::vector<std::byte> buffer(iris::network::BUFFER_SIZE);
    uint64_t received = 0;

    while (received < header->length)
    {
        auto want = static_cast<size_t>(std::min<uint64_t>(buffer.size(), header->length - received));

        auto got = this->socket->receive(buffer.data(), want);
        if (!got || *got == 0)
        {
            return cleanup(ClientError::ReceiveFailed); // premature end of stream
        }

        output.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(*got));
        if (!output)
        {
            return cleanup(ClientError::OutputFileFailed);
        }

        received += *got;
        LOG("Received " << received << "/" << header->length << " bytes");
    }

    PRINT("File \"" << filename << "\" received successfully (" << received << " bytes)");
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

    // Single loop multiplexing stdin and the socket with poll(), so a server
    // disconnect is noticed immediately instead of only after the next Enter.
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

        // Incoming chat lines (or a closed connection) from the server.
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

        // Keyboard input. Assemble whole lines so poll() stays correct even when
        // several arrive in one read.
        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            char buffer[4096];
            ssize_t got = ::read(STDIN_FILENO, buffer, sizeof(buffer));
            if (got <= 0)
            {
                break; // stdin closed (EOF / Ctrl-D)
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
                    message.resize(std::numeric_limits<uint16_t>::max());
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
