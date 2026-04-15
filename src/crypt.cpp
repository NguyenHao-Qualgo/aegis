#include "aegis/crypt.h"
#include "aegis/utils.h"

#include <cstdio>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>

namespace aegis {

std::string generate_aes_key() {
    unsigned char key[32]; // AES-256
    if (RAND_bytes(key, sizeof(key)) != 1)
        throw CryptError("Failed to generate random AES key");

    std::string hex;
    hex.reserve(64);
    for (int i = 0; i < 32; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", key[i]);
        hex += buf;
    }
    return hex;
}

static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

/// Encrypt file using AES-256-CBC with zero IV (dm-crypt plain64 mode).
/// dm-crypt operates on 512-byte sectors; each sector's IV is derived from
/// its sector number in plain64 mode.
std::string crypt_encrypt(const std::string& input_path, const std::string& output_path,
                          uint64_t data_size) {
    auto hex_key = generate_aes_key();
    auto key_bytes = hex_to_bytes(hex_key);

    FILE* fin = fopen(input_path.c_str(), "rb");
    if (!fin)
        throw CryptError("Cannot open input: " + input_path);

    FILE* fout = fopen(output_path.c_str(), "wb");
    if (!fout) {
        fclose(fin);
        throw CryptError("Cannot open output: " + output_path);
    }

    const size_t sector_size = 512;
    std::vector<uint8_t> sector_buf(sector_size);
    std::vector<uint8_t> enc_buf(sector_size + EVP_MAX_BLOCK_LENGTH);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    uint64_t sector_num = 0;
    uint64_t remaining = data_size;

    while (remaining > 0) {
        size_t to_read = std::min(static_cast<uint64_t>(sector_size), remaining);
        size_t rd = fread(sector_buf.data(), 1, to_read, fin);
        if (rd < to_read)
            std::memset(sector_buf.data() + rd, 0, to_read - rd);
        // Pad to full sector
        if (to_read < sector_size)
            std::memset(sector_buf.data() + to_read, 0, sector_size - to_read);

        // IV = sector number in little-endian (plain64 mode)
        uint8_t iv[16] = {};
        for (int i = 0; i < 8; ++i)
            iv[i] = static_cast<uint8_t>((sector_num >> (i * 8)) & 0xFF);

        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_bytes.data(), iv);
        EVP_CIPHER_CTX_set_padding(ctx, 0); // No padding, sector-aligned

        int out_len = 0;
        EVP_EncryptUpdate(ctx, enc_buf.data(), &out_len, sector_buf.data(), sector_size);
        int final_len = 0;
        EVP_EncryptFinal_ex(ctx, enc_buf.data() + out_len, &final_len);
        out_len += final_len;

        fwrite(enc_buf.data(), 1, out_len, fout);
        remaining -= to_read;
        sector_num++;
    }

    EVP_CIPHER_CTX_free(ctx);
    fclose(fin);
    fclose(fout);

    LOG_INFO("Encrypted %lu bytes (%lu sectors)", data_size, sector_num);
    return hex_key;
}

Result<void> crypt_decrypt(const std::string& input_path, const std::string& output_path,
                           const std::string& hex_key, uint64_t data_size) {
    auto key_bytes = hex_to_bytes(hex_key);
    if (key_bytes.size() != 32)
        return Result<void>::err("Invalid AES-256 key length");

    FILE* fin = fopen(input_path.c_str(), "rb");
    if (!fin)
        return Result<void>::err("Cannot open encrypted input: " + input_path);

    FILE* fout = fopen(output_path.c_str(), "wb");
    if (!fout) {
        fclose(fin);
        return Result<void>::err("Cannot open decrypted output: " + output_path);
    }

    const size_t sector_size = 512;
    std::vector<uint8_t> sector_buf(sector_size);
    std::vector<uint8_t> dec_buf(sector_size + EVP_MAX_BLOCK_LENGTH);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    uint64_t sector_num = 0;
    uint64_t remaining = data_size;

    while (remaining > 0) {
        size_t to_read = std::min(static_cast<uint64_t>(sector_size), remaining);
        size_t rd = fread(sector_buf.data(), 1, sector_size, fin);
        if (rd < sector_size)
            std::memset(sector_buf.data() + rd, 0, sector_size - rd);

        uint8_t iv[16] = {};
        for (int i = 0; i < 8; ++i)
            iv[i] = static_cast<uint8_t>((sector_num >> (i * 8)) & 0xFF);

        EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_bytes.data(), iv);
        EVP_CIPHER_CTX_set_padding(ctx, 0);

        int out_len = 0;
        EVP_DecryptUpdate(ctx, dec_buf.data(), &out_len, sector_buf.data(), sector_size);
        int final_len = 0;
        EVP_DecryptFinal_ex(ctx, dec_buf.data() + out_len, &final_len);
        out_len += final_len;

        size_t to_write = std::min(static_cast<uint64_t>(out_len), remaining);
        fwrite(dec_buf.data(), 1, to_write, fout);
        remaining -= to_read;
        sector_num++;
    }

    EVP_CIPHER_CTX_free(ctx);
    fclose(fin);
    fclose(fout);

    return Result<void>::ok();
}

} // namespace aegis
