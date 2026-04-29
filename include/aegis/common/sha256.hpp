#pragma once

#include <openssl/evp.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

namespace aegis {

class Sha256 {
public:
    Sha256() {
        reset();
    }

    void reset() {
        ctx_.reset(EVP_MD_CTX_new());
        if (!ctx_) {
            throw std::runtime_error("EVP_MD_CTX_new failed");
        }

        if (EVP_DigestInit_ex(ctx_.get(), EVP_sha256(), nullptr) != 1) {
            throw std::runtime_error("EVP_DigestInit_ex SHA-256 failed");
        }

        finalized_ = false;
    }

    void update(const unsigned char* data, std::size_t len) {
        if (finalized_) {
            throw std::runtime_error("Sha256 update after final");
        }

        if (len == 0) {
            return;
        }

        if (EVP_DigestUpdate(ctx_.get(), data, len) != 1) {
            throw std::runtime_error("EVP_DigestUpdate SHA-256 failed");
        }
    }

    void update(const char* data, std::size_t len) {
        update(reinterpret_cast<const unsigned char*>(data), len);
    }

    std::string final_hex() {
        if (finalized_) {
            throw std::runtime_error("Sha256 finalized twice");
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int digest_len = 0;

        if (EVP_DigestFinal_ex(ctx_.get(), digest.data(), &digest_len) != 1) {
            throw std::runtime_error("EVP_DigestFinal_ex SHA-256 failed");
        }

        finalized_ = true;

        static constexpr char kHex[] = "0123456789abcdef";

        std::string out;
        out.reserve(digest_len * 2);

        for (unsigned int i = 0; i < digest_len; ++i) {
            const unsigned char byte = digest[i];
            out.push_back(kHex[(byte >> 4U) & 0x0FU]);
            out.push_back(kHex[byte & 0x0FU]);
        }

        return out;
    }

private:
    struct EvpMdCtxDeleter {
        void operator()(EVP_MD_CTX* ctx) const {
            EVP_MD_CTX_free(ctx);
        }
    };

    std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter> ctx_;
    bool finalized_ = false;
};

}  // namespace aegis