#include "protocol.h"

#include <cstring>

namespace iris::protocol
{
    static void write_u16(std::byte *out, uint16_t value)
    {
        out[0] = static_cast<std::byte>((value >> 8) & 0xFF);
        out[1] = static_cast<std::byte>(value & 0xFF);
    }

    static uint16_t read_u16(const std::byte *in)
    {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(std::to_integer<uint8_t>(in[0])) << 8) |
            static_cast<uint16_t>(std::to_integer<uint8_t>(in[1])));
    }

    static void write_u64(std::byte *out, uint64_t value)
    {
        for (int i = 0; i < 8; i++)
        {
            out[i] = static_cast<std::byte>((value >> (8 * (7 - i))) & 0xFF);
        }
    }

    static uint64_t read_u64(const std::byte *in)
    {
        uint64_t value = 0;
        for (int i = 0; i < 8; i++)
        {
            value = (value << 8) | std::to_integer<uint8_t>(in[i]);
        }
        return value;
    }

    std::vector<std::byte> serialize_request(const std::string &filename)
    {
        auto nameLength = static_cast<uint16_t>(filename.size());

        std::vector<std::byte> frame(REQUEST_HEADER_SIZE + filename.size());
        write_u16(frame.data(), nameLength);
        std::memcpy(frame.data() + REQUEST_HEADER_SIZE, filename.data(), filename.size());

        return frame;
    }

    std::optional<uint16_t> parse_request_header(const std::byte *data, size_t length)
    {
        if (length < REQUEST_HEADER_SIZE)
        {
            return std::nullopt;
        }

        return read_u16(data);
    }

    std::vector<std::byte> serialize_response_header(Status status, uint64_t length)
    {
        std::vector<std::byte> header(RESPONSE_HEADER_SIZE);
        header[0] = static_cast<std::byte>(status);
        write_u64(header.data() + sizeof(uint8_t), length);

        return header;
    }

    std::optional<ResponseHeader> parse_response_header(const std::byte *data, size_t length)
    {
        if (length < RESPONSE_HEADER_SIZE)
        {
            return std::nullopt;
        }

        ResponseHeader header{};
        header.status = static_cast<Status>(std::to_integer<uint8_t>(data[0]));
        header.length = read_u64(data + sizeof(uint8_t));

        return header;
    }
}  // namespace iris::protocol
