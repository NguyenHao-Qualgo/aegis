#pragma once

#include <array>
#include <memory>
#include <vector>

#include <openssl/err.h>
#include <openssl/evp.h>

#include "aegis/io/cpio.hpp"
#include "aegis/crypto/crypto.hpp"
#include "aegis/common/logging.hpp"
#include "aegis/crypto/sha256.hpp"
#include "aegis/core/types.hpp"

namespace aegis {

template <typename Sink>
void stream_plain_payload(StreamReader &reader,
                          const CpioEntry &entry,
                          Sink sink,
                          const std::string &expected_sha256) {
    std::array<char, kIoBufferSize> buffer{};
    std::uint64_t remaining = entry.size;
    std::uint32_t checksum  = 0;
    Sha256 sha;

    LOG_I("streaming payload '" + entry.name + "' directly from SWU stream, size=" +
               std::to_string(entry.size) + " bytes");

    while (remaining > 0) {
        const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
        reader.read_exact(buffer.data(), chunk);
        for (std::size_t i = 0; i < chunk; ++i) { checksum += static_cast<unsigned char>(buffer[i]); }
        sha.update(buffer.data(), chunk);
        sink(buffer.data(), chunk);
        remaining -= chunk;
    }

    if (checksum != entry.checksum) { fail_runtime("cpio checksum mismatch for " + entry.name); }
    skip_padding(reader, entry.size);
    if (!expected_sha256.empty() && sha.final_hex() != expected_sha256) {
        fail_runtime("sha256 mismatch for " + entry.name);
    }
    LOG_I("finished streaming payload '" + entry.name + "', streamed=" +
               std::to_string(entry.size) + " bytes");
}

template <typename Sink>
void stream_encrypted_payload(StreamReader &reader,
                              const CpioEntry &entry,
                              const AesMaterial &aes,
                              const std::string &ivt_override,
                              Sink sink,
                              const std::string &expected_sha256) {
    const std::vector<unsigned char> key    = hex_to_bytes(aes.key_hex);
    const std::vector<unsigned char> iv     = hex_to_bytes(ivt_override.empty() ? aes.iv_hex : ivt_override);
    const EVP_CIPHER                *cipher = evp_cipher_for_key_length(key.size());
    if (cipher == nullptr) { fail_runtime("unsupported AES key length for encrypted payload"); }
    if (iv.size() != static_cast<std::size_t>(EVP_CIPHER_iv_length(cipher))) {
        fail_runtime("invalid IV length for encrypted payload");
    }

    using EvpCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EvpCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) { fail_runtime("EVP_CIPHER_CTX_new failed"); }
    if (EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, key.data(), iv.data()) != 1) {
        const std::string errors = collect_openssl_errors();
        fail_runtime("EVP_DecryptInit_ex failed" + (errors.empty() ? std::string() : "\n" + errors));
    }

    LOG_I("streaming encrypted payload '" + entry.name +
               "' through in-process OpenSSL AES-CBC decrypt without extracting SWU");

    std::array<char, kIoBufferSize> inbuf{};
    std::vector<unsigned char> outbuf(kIoBufferSize + EVP_CIPHER_block_size(cipher));
    std::uint64_t remaining    = entry.size;
    std::uint32_t checksum     = 0;
    Sha256 encrypted_sha;

    while (remaining > 0) {
        const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, inbuf.size()));
        reader.read_exact(inbuf.data(), chunk);
        for (std::size_t i = 0; i < chunk; ++i) { checksum += static_cast<unsigned char>(inbuf[i]); }
        encrypted_sha.update(inbuf.data(), chunk);

        int outlen = 0;
        if (EVP_DecryptUpdate(ctx.get(), outbuf.data(), &outlen,
                              reinterpret_cast<const unsigned char *>(inbuf.data()),
                              static_cast<int>(chunk)) != 1) {
            const std::string errors = collect_openssl_errors();
            fail_runtime("EVP_DecryptUpdate failed" + (errors.empty() ? std::string() : "\n" + errors));
        }
        if (outlen > 0) { sink(reinterpret_cast<const char *>(outbuf.data()), static_cast<std::size_t>(outlen)); }
        remaining -= chunk;
    }

    if (checksum != entry.checksum) { fail_runtime("cpio checksum mismatch for " + entry.name); }
    skip_padding(reader, entry.size);

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), outbuf.data(), &final_len) != 1) {
        const std::string errors = collect_openssl_errors();
        fail_runtime("EVP_DecryptFinal_ex failed" + (errors.empty() ? std::string() : "\n" + errors));
    }
    if (final_len > 0) { sink(reinterpret_cast<const char *>(outbuf.data()), static_cast<std::size_t>(final_len)); }

    if (!expected_sha256.empty() && encrypted_sha.final_hex() != expected_sha256) {
        fail_runtime("sha256 mismatch for " + entry.name);
    }
    LOG_I("finished decrypted streaming for payload '" + entry.name + "'");
}

}  // namespace aegis
