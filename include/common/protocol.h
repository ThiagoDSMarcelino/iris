#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace iris::protocol
{
    enum class Status : uint8_t
    {
        OK = 0,
        NOT_FOUND = 1,
        ERROR = 2,
    };

    enum class MessageType : uint8_t
    {
        FileRequest = 0,
        ChatJoin = 1,
        ChatMessage = 2,
    };

    constexpr size_t OPCODE_SIZE = sizeof(uint8_t);
    constexpr size_t LENGTH_PREFIX_SIZE = sizeof(uint16_t);
    constexpr size_t RESPONSE_HEADER_SIZE = sizeof(uint8_t) + sizeof(uint64_t);

    struct ResponseHeader
    {
        Status status;
        uint64_t length;
    };

    std::optional<MessageType> parse_opcode(const std::byte *data, size_t length);
    std::optional<uint16_t> parse_u16_length(const std::byte *data, size_t length);

    std::vector<std::byte> serialize_file_request(const std::string &filename);
    std::vector<std::byte> serialize_chat_join(const std::string &nick, const std::string &room);
    std::vector<std::byte> serialize_chat_message(const std::string &text);

    std::vector<std::byte> serialize_chat_line(const std::string &text);

    std::vector<std::byte> serialize_response_header(Status status, uint64_t length);
    std::optional<ResponseHeader> parse_response_header(const std::byte *data, size_t length);
} // namespace iris::protocol
