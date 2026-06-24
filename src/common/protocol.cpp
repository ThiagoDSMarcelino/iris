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

    std::optional<MessageType> parse_opcode(const std::byte *data, size_t length)
    {
        if (length < OPCODE_SIZE)
        {
            return std::nullopt;
        }

        return static_cast<MessageType>(std::to_integer<uint8_t>(data[0]));
    }

    std::optional<uint16_t> parse_u16_length(const std::byte *data, size_t length)
    {
        if (length < LENGTH_PREFIX_SIZE)
        {
            return std::nullopt;
        }

        return read_u16(data);
    }

    std::vector<std::byte> serialize_file_request(const std::string &filename)
    {
        auto nameLength = static_cast<uint16_t>(filename.size());

        std::vector<std::byte> frame(OPCODE_SIZE + LENGTH_PREFIX_SIZE + filename.size());
        frame[0] = static_cast<std::byte>(MessageType::FileRequest);
        write_u16(frame.data() + OPCODE_SIZE, nameLength);
        std::memcpy(frame.data() + OPCODE_SIZE + LENGTH_PREFIX_SIZE, filename.data(), filename.size());

        return frame;
    }

    std::vector<std::byte> serialize_chat_join(const std::string &nick, const std::string &room)
    {
        auto nickLength = static_cast<uint16_t>(nick.size());
        auto roomLength = static_cast<uint16_t>(room.size());

        std::vector<std::byte> frame(
            OPCODE_SIZE + LENGTH_PREFIX_SIZE + nick.size() + LENGTH_PREFIX_SIZE + room.size());

        size_t offset = 0;
        frame[offset] = static_cast<std::byte>(MessageType::ChatJoin);
        offset += OPCODE_SIZE;

        write_u16(frame.data() + offset, nickLength);
        offset += LENGTH_PREFIX_SIZE;
        std::memcpy(frame.data() + offset, nick.data(), nick.size());
        offset += nick.size();

        write_u16(frame.data() + offset, roomLength);
        offset += LENGTH_PREFIX_SIZE;
        std::memcpy(frame.data() + offset, room.data(), room.size());

        return frame;
    }

    std::vector<std::byte> serialize_chat_message(const std::string &text)
    {
        auto textLength = static_cast<uint16_t>(text.size());

        std::vector<std::byte> frame(OPCODE_SIZE + LENGTH_PREFIX_SIZE + text.size());
        frame[0] = static_cast<std::byte>(MessageType::ChatMessage);
        write_u16(frame.data() + OPCODE_SIZE, textLength);
        std::memcpy(frame.data() + OPCODE_SIZE + LENGTH_PREFIX_SIZE, text.data(), text.size());

        return frame;
    }

    std::vector<std::byte> serialize_chat_line(const std::string &text)
    {
        auto textLength = static_cast<uint16_t>(text.size());

        std::vector<std::byte> frame(LENGTH_PREFIX_SIZE + text.size());
        write_u16(frame.data(), textLength);
        std::memcpy(frame.data() + LENGTH_PREFIX_SIZE, text.data(), text.size());

        return frame;
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
