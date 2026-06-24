#include "socket.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

using iris::network::Socket;

Socket::~Socket()
{
    if (this->fileDescriptor >= 0)
    {
        close(this->fileDescriptor);
    }
}

std::optional<std::unique_ptr<Socket>> Socket::create()
{
    int sockfd = socket(
        AF_INET,     // IPv4
        SOCK_STREAM, // TCP
        0            // Default protocol
    );

    if (sockfd < 0)
    {
        return std::nullopt;
    }

    auto sock = std::unique_ptr<Socket>(new Socket());
    sock->fileDescriptor = sockfd;

    return sock;
}

bool Socket::bind(uint16_t port)
{
    int reuse = 1;
    setsockopt(this->fileDescriptor, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;

    return ::bind(this->fileDescriptor, reinterpret_cast<sockaddr *>(&local), sizeof(local)) == 0;
}

bool Socket::listen(int backlog)
{
    return ::listen(this->fileDescriptor, backlog) == 0;
}

std::optional<std::unique_ptr<Socket>> Socket::accept()
{
    sockaddr_in peer{};
    socklen_t peerLen = sizeof(peer);

    int connfd = ::accept(this->fileDescriptor, reinterpret_cast<sockaddr *>(&peer), &peerLen);
    if (connfd < 0)
    {
        return std::nullopt;
    }

    auto sock = std::unique_ptr<Socket>(new Socket());
    sock->fileDescriptor = connfd;
    sock->peerPort = ntohs(peer.sin_port);
    inet_ntop(AF_INET, &peer.sin_addr, sock->peerIp, sizeof(sock->peerIp));

    return sock;
}

bool Socket::connect(const char *ip, uint16_t port)
{
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &dest.sin_addr) <= 0)
    {
        return false;
    }

    return ::connect(this->fileDescriptor, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) == 0;
}

bool Socket::send_all(const void *data, size_t length)
{
    const auto *cursor = static_cast<const std::byte *>(data);
    size_t sent = 0;

    while (sent < length)
    {
        ssize_t written = ::send(this->fileDescriptor, cursor + sent, length - sent, MSG_NOSIGNAL);
        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        sent += static_cast<size_t>(written);
    }

    return true;
}

std::optional<size_t> Socket::receive(void *buffer, size_t length)
{
    while (true)
    {
        ssize_t got = ::recv(this->fileDescriptor, buffer, length, 0);
        if (got < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return std::nullopt;
        }
        return static_cast<size_t>(got); // 0 == peer performed an orderly shutdown
    }
}

bool Socket::receive_all(void *buffer, size_t length)
{
    auto *cursor = static_cast<std::byte *>(buffer);
    size_t received = 0;

    while (received < length)
    {
        auto got = this->receive(cursor + received, length - received);
        if (!got || *got == 0)
        {
            return false; // error or premature end of stream
        }
        received += *got;
    }

    return true;
}

int Socket::native_handle() const
{
    return this->fileDescriptor;
}

const char *Socket::peer_ip() const
{
    return this->peerIp;
}

uint16_t Socket::peer_port() const
{
    return this->peerPort;
}
