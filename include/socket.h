#pragma once

#include <cstddef>
#include <expected>
#include <netinet/in.h>
#include <memory>
#include <optional>

namespace luft::network
{
    enum class ReceiveError
    {
        Timeout,
        Failed,
    };
    constexpr size_t BUFFER_SIZE = 1024; // 1 KB

    struct Message
    {
        std::byte data[BUFFER_SIZE];
        char senderIp[INET_ADDRSTRLEN];
        size_t size;
        uint16_t senderPort;
    };

    class Socket
    {
    public:
        ~Socket();

        static std::optional<std::unique_ptr<Socket>> create();

        bool send(
            const char *ip, uint16_t port,
            const void *data, size_t length);

        bool bind(uint16_t port);

        std::optional<Message> receive();
        std::expected<Message, ReceiveError> receive_with_timeout(uint32_t milliseconds);

    private:
        Socket() = default;
        int fileDescriptor;
    };
}  // namespace luft::network