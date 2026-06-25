#include "sha256.h"

#include <openssl/evp.h>

#include <cstdint>
#include <stdexcept>

namespace iris::crypto
{
    Sha256::Sha256() : context(EVP_MD_CTX_new())
    {
        auto *ctx = static_cast<EVP_MD_CTX *>(context);
        if (ctx == nullptr || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
        {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize SHA-256 context");
        }
    }

    Sha256::~Sha256()
    {
        EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(context));
    }

    void Sha256::update(const void *data, size_t length)
    {
        EVP_DigestUpdate(static_cast<EVP_MD_CTX *>(context), data, length);
    }

    Sha256Digest Sha256::finalize()
    {
        Sha256Digest digest{};
        unsigned int length = 0;
        EVP_DigestFinal_ex(static_cast<EVP_MD_CTX *>(context),
                           reinterpret_cast<unsigned char *>(digest.data()), &length);
        return digest;
    }

    std::string to_hex(const Sha256Digest &digest)
    {
        static constexpr char hexChars[] = "0123456789abcdef";

        std::string result;
        result.reserve(digest.size() * 2);
        for (std::byte b : digest)
        {
            auto value = std::to_integer<uint8_t>(b);
            result.push_back(hexChars[value >> 4]);
            result.push_back(hexChars[value & 0x0F]);
        }

        return result;
    }
} // namespace iris::crypto
