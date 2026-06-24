#include "socket.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

using luft::network::Message;
using luft::network::ReceiveError;
using luft::network::Socket;

Socket::~Socket()
{
    close(this->fileDescriptor);
}

std::optional<std::unique_ptr<Socket>> Socket::create()
{
    int sockfd = socket(
        AF_INET,    // IPv4
        SOCK_DGRAM, // UDP
        0           // Default protocol
    );

    if (sockfd < 0)
    {
        return std::nullopt;
    }

    auto sock = std::unique_ptr<Socket>(new Socket());
    sock->fileDescriptor = sockfd;

    return sock;
}

bool Socket::send(const char *ip, uint16_t port, const void *data, size_t length)
{
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, ip, &dest.sin_addr);

    ssize_t sent = sendto(
        this->fileDescriptor, data, length, 0,
        reinterpret_cast<sockaddr *>(&dest), sizeof(dest));

    return sent >= 0;
}

bool Socket::bind(uint16_t port)
{
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;

    return ::bind(this->fileDescriptor, reinterpret_cast<sockaddr *>(&local), sizeof(local)) == 0;
}

static std::optional<Message> do_receive(int fd)
{
    Message message{};
    sockaddr_in sender{};
    socklen_t sender_len = sizeof(sender);

    ssize_t received = recvfrom(
        fd,
        message.data, sizeof(message.data), 0,
        reinterpret_cast<sockaddr *>(&sender), &sender_len);

    if (received < 0)
    {
        return std::nullopt;
    }

    message.size = received;
    message.senderPort = ntohs(sender.sin_port);
    inet_ntop(AF_INET, &sender.sin_addr, message.senderIp, sizeof(message.senderIp));

    return message;
}

std::optional<Message> Socket::receive()
{
    return do_receive(this->fileDescriptor);
}

std::expected<Message, ReceiveError> Socket::receive_with_timeout(uint32_t milliseconds)
{
    timeval tv{};
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;

    setsockopt(this->fileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    auto result = do_receive(this->fileDescriptor);
    int savedErrno = errno;

    timeval zero{};
    setsockopt(this->fileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &zero, sizeof(zero));

    if (result)
    {
        return *result;
    }

    if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
    {
        return std::unexpected(ReceiveError::Timeout);
    }

    return std::unexpected(ReceiveError::Failed);
}
