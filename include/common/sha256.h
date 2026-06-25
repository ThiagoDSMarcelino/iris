#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace iris::crypto
{
    constexpr size_t SHA256_DIGEST_SIZE = 32;

    using Sha256Digest = std::array<std::byte, SHA256_DIGEST_SIZE>;

    class Sha256
    {
    public:
        Sha256();
        ~Sha256();

        Sha256(const Sha256 &) = delete;
        Sha256 &operator=(const Sha256 &) = delete;

        void update(const void *data, size_t length);
        Sha256Digest finalize();

    private:
        void *context;
    };

    std::string to_hex(const Sha256Digest &digest);
} // namespace iris::crypto
