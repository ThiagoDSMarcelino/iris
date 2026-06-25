#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <optional>

namespace iris::network
{
    constexpr size_t BUFFER_SIZE = 64 * 1024;

    class Socket
    {
    public:
        ~Socket();

        Socket(const Socket &) = delete;
        Socket &operator=(const Socket &) = delete;

        static std::optional<std::unique_ptr<Socket>> create();

        bool bind(uint16_t port);
        bool listen(int backlog = 16);
        std::optional<std::unique_ptr<Socket>> accept();

        bool connect(const char *ip, uint16_t port);

        bool send_all(const void *data, size_t length);
        std::optional<size_t> receive(void *buffer, size_t length);
        bool receive_all(void *buffer, size_t length);

        int native_handle() const;

        const char *peer_ip() const;
        uint16_t peer_port() const;

    private:
        Socket() = default;

        int fileDescriptor = -1;
        char peerIp[INET_ADDRSTRLEN] = {};
        uint16_t peerPort = 0;
    };
} // namespace iris::network
