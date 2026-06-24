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

    constexpr size_t REQUEST_HEADER_SIZE = sizeof(uint16_t);                    // 2
    constexpr size_t RESPONSE_HEADER_SIZE = sizeof(uint8_t) + sizeof(uint64_t); // 9

    struct ResponseHeader
    {
        Status status;
        uint64_t length;
    };

    // Request
    std::vector<std::byte> serialize_request(const std::string &filename);
    std::optional<uint16_t> parse_request_header(const std::byte *data, size_t length);

    // Response
    std::vector<std::byte> serialize_response_header(Status status, uint64_t length);
    std::optional<ResponseHeader> parse_response_header(const std::byte *data, size_t length);
} // namespace iris::protocol
