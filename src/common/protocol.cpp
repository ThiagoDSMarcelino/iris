#include "protocol.h"

#include <arpa/inet.h>
#include <cstring>
#include <zlib.h>

#include "log.h"

namespace luft::protocol
{
    std::vector<std::byte> serialize(uint8_t seq, Code code, uint32_t total, const std::vector<std::byte> &payload)
    {
        std::vector<std::byte> packet(HEADER_SIZE + payload.size());

        size_t offset = 0;

        packet[offset] = static_cast<std::byte>(seq);
        offset += sizeof(seq);

        packet[offset] = static_cast<std::byte>(code);
        offset += sizeof(code);

        uint32_t netTotal = htonl(total);
        std::memcpy(packet.data() + offset, &netTotal, sizeof(netTotal));
        offset += sizeof(netTotal);

        uint16_t netSize = htons(static_cast<uint16_t>(payload.size()));
        std::memcpy(packet.data() + offset, &netSize, sizeof(netSize));
        offset += sizeof(netSize);

        uint32_t checksum = calculate_checksum(payload);
        uint32_t netChecksum = htonl(checksum);
        std::memcpy(packet.data() + offset, &netChecksum, sizeof(netChecksum));
        offset += sizeof(netChecksum);

        std::memcpy(packet.data() + offset, payload.data(), payload.size());

        return packet;
    }

    std::optional<Packet> deserialize(const std::byte *data, size_t length)
    {
        if (length < HEADER_SIZE)
        {
            return std::nullopt;
        }

        Packet packet{};
        size_t offset = 0;

        packet.seq = static_cast<uint8_t>(data[offset]);
        offset += sizeof(packet.seq);

        packet.code = static_cast<uint8_t>(data[offset]);
        offset += sizeof(packet.code);

        uint32_t netTotal;
        std::memcpy(&netTotal, data + offset, sizeof(netTotal));
        packet.total = ntohl(netTotal);
        offset += sizeof(netTotal);

        uint16_t netSize;
        std::memcpy(&netSize, data + offset, sizeof(netSize));
        packet.size = ntohs(netSize);
        offset += sizeof(netSize);

        uint32_t netChecksum;
        std::memcpy(&netChecksum, data + offset, sizeof(netChecksum));
        packet.checksum = ntohl(netChecksum);
        offset += sizeof(netChecksum);

        packet.payload.assign(data + offset, data + length);

        uint32_t expectedChecksum = calculate_checksum(packet.payload);

        if (packet.checksum != expectedChecksum)
        {
            LOG_ERR("Checksum mismatch: expected " << expectedChecksum << ", got " << packet.checksum);
            return std::nullopt;
        }

        return packet;
    }

    uint32_t calculate_checksum(const std::vector<std::byte> &packet)
    {
        return crc32(0, reinterpret_cast<const uint8_t *>(packet.data()), packet.size());
    }
} // namespace luft::protocol
