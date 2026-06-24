#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace luft::protocol
{
    // Wire format: [ seq (1 byte) | code (1 byte) | total (4 bytes) | size (2 bytes) | checksum (4 bytes) | payload (size bytes) ]
    constexpr size_t HEADER_SIZE = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint32_t);

    struct Packet
    {
        uint8_t seq;
        uint8_t code;
        uint32_t total;
        uint16_t size;
        uint32_t checksum;
        std::vector<std::byte> payload;
    };

    enum class Code : uint8_t
    {
        DATA,
        ERROR,
        ACK,
        NACK,
    };

    std::vector<std::byte> serialize(uint8_t seq, Code code, uint32_t total, const std::vector<std::byte> &payload);
    std::optional<Packet> deserialize(const std::byte *data, size_t length);
    uint32_t calculate_checksum(const std::vector<std::byte> &packet);
}  // namespace luft::protocol