#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace aegis {

class Sha256 {
public:
    Sha256() { reset(); }

    void reset() {
        state_ = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                  0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
        bit_len_ = 0;
        buffer_len_ = 0;
    }

    void update(const unsigned char *data, std::size_t len) {
        bit_len_ += static_cast<std::uint64_t>(len) * 8U;
        for (std::size_t i = 0; i < len; ++i) {
            buffer_[buffer_len_++] = data[i];
            if (buffer_len_ == buffer_.size()) {
                transform(buffer_.data());
                buffer_len_ = 0;
            }
        }
    }

    void update(const char *data, std::size_t len) {
        update(reinterpret_cast<const unsigned char *>(data), len);
    }

    std::string final_hex() {
        std::array<unsigned char, 32> digest{};
        finalize(digest);
        static constexpr char kHex[] = "0123456789abcdef";
        std::string out;
        out.reserve(digest.size() * 2);
        for (unsigned char byte : digest) {
            out.push_back(kHex[(byte >> 4U) & 0xF]);
            out.push_back(kHex[byte & 0xF]);
        }
        return out;
    }

private:
    static constexpr std::array<std::uint32_t, 64> kTable = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

    static inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
        return (x >> n) | (x << (32U - n));
    }

    void transform(const unsigned char *chunk) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            const int j = i * 4;
            w[i] = (static_cast<std::uint32_t>(chunk[j]) << 24U) |
                   (static_cast<std::uint32_t>(chunk[j + 1]) << 16U) |
                   (static_cast<std::uint32_t>(chunk[j + 2]) << 8U) |
                   static_cast<std::uint32_t>(chunk[j + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7U) ^ rotr(w[i - 15], 18U) ^ (w[i - 15] >> 3U);
            const std::uint32_t s1 = rotr(w[i - 2], 17U) ^ rotr(w[i - 2], 19U) ^ (w[i - 2] >> 10U);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        auto a = state_[0]; auto b = state_[1]; auto c = state_[2]; auto d = state_[3];
        auto e = state_[4]; auto f = state_[5]; auto g = state_[6]; auto h = state_[7];

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1  = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
            const std::uint32_t ch  = (e & f) ^ ((~e) & g);
            const std::uint32_t t1  = h + s1 + ch + kTable[i] + w[i];
            const std::uint32_t s0  = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2  = s0 + maj;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    void finalize(std::array<unsigned char, 32> &digest) {
        buffer_[buffer_len_++] = 0x80;
        if (buffer_len_ > 56) {
            while (buffer_len_ < 64) { buffer_[buffer_len_++] = 0; }
            transform(buffer_.data());
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) { buffer_[buffer_len_++] = 0; }
        for (int i = 7; i >= 0; --i) {
            buffer_[buffer_len_++] = static_cast<unsigned char>((bit_len_ >> (i * 8)) & 0xFFU);
        }
        transform(buffer_.data());
        for (std::size_t i = 0; i < state_.size(); ++i) {
            digest[i * 4]     = static_cast<unsigned char>((state_[i] >> 24U) & 0xFFU);
            digest[i * 4 + 1] = static_cast<unsigned char>((state_[i] >> 16U) & 0xFFU);
            digest[i * 4 + 2] = static_cast<unsigned char>((state_[i] >> 8U)  & 0xFFU);
            digest[i * 4 + 3] = static_cast<unsigned char>(state_[i] & 0xFFU);
        }
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<unsigned char, 64> buffer_{};
    std::uint64_t bit_len_ = 0;
    std::size_t buffer_len_ = 0;
};

}  // namespace aegis
