#include "aegis/checksum.h"
#include "aegis/error.h"

#include <cstdio>
#include <openssl/evp.h>

namespace aegis {

std::string sha256_hex(const void* data, size_t len) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw ChecksumError("EVP_MD_CTX_new failed");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw ChecksumError("SHA-256 computation failed");
    }
    EVP_MD_CTX_free(ctx);

    std::string hex;
    hex.reserve(hash_len * 2);
    for (unsigned i = 0; i < hash_len; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hex += buf;
    }
    return hex;
}

Checksum compute_checksum(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) throw ChecksumError("Cannot open file: " + path);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    uint64_t total = 0;
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        EVP_DigestUpdate(ctx, buf, n);
        total += n;
    }
    fclose(f);

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    Checksum cs;
    cs.size = total;
    cs.digest.reserve(hash_len * 2);
    for (unsigned i = 0; i < hash_len; ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", hash[i]);
        cs.digest += hex;
    }
    return cs;
}

bool verify_checksum(const std::string& path,
                     const std::string& expected_digest,
                     uint64_t expected_size) {
    auto cs = compute_checksum(path);
    return cs.digest == expected_digest &&
           (expected_size == 0 || cs.size == expected_size);
}

} // namespace aegis
