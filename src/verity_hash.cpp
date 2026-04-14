#include "aegis/verity_hash.h"
#include "aegis/checksum.h"
#include "aegis/utils.h"

#include <cstdio>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>

namespace aegis {

std::string generate_salt(size_t bytes) {
    std::vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), static_cast<int>(bytes)) != 1)
        throw AegisError("RAND_bytes failed for salt generation");

    std::string hex;
    hex.reserve(bytes * 2);
    for (auto b : buf) {
        char h[3];
        snprintf(h, sizeof(h), "%02x", b);
        hex += h;
    }
    return hex;
}

static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t b = static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16));
        bytes.push_back(b);
    }
    return bytes;
}

static std::string bytes_to_hex(const uint8_t* data, size_t len) {
    std::string hex;
    hex.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        char h[3];
        snprintf(h, sizeof(h), "%02x", data[i]);
        hex += h;
    }
    return hex;
}

/// Compute the verity hash tree following the Linux dm-verity algorithm.
/// Each level hashes pairs of blocks from the level below, with the
/// lowest level hashing data blocks. Salt is prepended to each hash input.
VerityResult compute_verity_hash(const std::string& bundle_path,
                                 uint64_t data_size,
                                 const std::string& salt_hex) {
    const uint32_t block_size = 4096;
    const uint32_t digest_size = 32; // SHA-256
    const uint32_t hashes_per_block = block_size / digest_size;

    std::string salt_str = salt_hex.empty() ? generate_salt(32) : salt_hex;
    auto salt = hex_to_bytes(salt_str);

    FILE* f = fopen(bundle_path.c_str(), "r+b");
    if (!f) throw AegisError("Cannot open bundle for verity: " + bundle_path);

    // Compute number of data blocks
    uint64_t num_data_blocks = (data_size + block_size - 1) / block_size;

    // Build hash tree bottom-up
    // Level 0: hash of each data block
    // Level n+1: hash of groups of hashes_per_block from level n
    std::vector<std::vector<uint8_t>> levels;

    // Level 0: hash all data blocks
    {
        std::vector<uint8_t> level0;
        std::vector<uint8_t> block_buf(block_size, 0);
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();

        for (uint64_t i = 0; i < num_data_blocks; ++i) {
            fseek(f, static_cast<long>(i * block_size), SEEK_SET);
            size_t to_read = block_size;
            if (i == num_data_blocks - 1 && data_size % block_size != 0) {
                to_read = data_size % block_size;
                std::memset(block_buf.data() + to_read, 0, block_size - to_read);
            }
            size_t rd = fread(block_buf.data(), 1, to_read, f);
            if (rd < to_read)
                std::memset(block_buf.data() + rd, 0, to_read - rd);

            uint8_t hash[32];
            EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
            EVP_DigestUpdate(ctx, salt.data(), salt.size());
            EVP_DigestUpdate(ctx, block_buf.data(), block_size);
            unsigned int hl;
            EVP_DigestFinal_ex(ctx, hash, &hl);

            level0.insert(level0.end(), hash, hash + digest_size);
        }
        EVP_MD_CTX_free(ctx);
        levels.push_back(std::move(level0));
    }

    // Build upper levels
    while (levels.back().size() > digest_size) {
        auto& prev = levels.back();
        uint64_t num_hashes = prev.size() / digest_size;
        uint64_t num_blocks_needed = (num_hashes + hashes_per_block - 1) / hashes_per_block;

        std::vector<uint8_t> next_level;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        std::vector<uint8_t> zero_block(block_size, 0);

        for (uint64_t b = 0; b < num_blocks_needed; ++b) {
            // Gather one block's worth of hashes
            std::vector<uint8_t> hash_block(block_size, 0);
            uint64_t start = b * hashes_per_block * digest_size;
            uint64_t end = std::min(start + block_size, static_cast<uint64_t>(prev.size()));
            std::memcpy(hash_block.data(), prev.data() + start, end - start);

            uint8_t hash[32];
            EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
            EVP_DigestUpdate(ctx, salt.data(), salt.size());
            EVP_DigestUpdate(ctx, hash_block.data(), block_size);
            unsigned int hl;
            EVP_DigestFinal_ex(ctx, hash, &hl);

            next_level.insert(next_level.end(), hash, hash + digest_size);
        }
        EVP_MD_CTX_free(ctx);
        levels.push_back(std::move(next_level));
    }

    // Write hash tree to end of bundle (levels in reverse order, level 0 last)
    fseek(f, 0, SEEK_END);
    uint64_t hash_tree_start = ftell(f);
    uint64_t total_hash_size = 0;

    for (auto it = levels.rbegin(); it != levels.rend(); ++it) {
        // Pad each level to block boundary
        size_t padded_size = ((it->size() + block_size - 1) / block_size) * block_size;
        it->resize(padded_size, 0);
        fwrite(it->data(), 1, it->size(), f);
        total_hash_size += it->size();
    }
    fclose(f);

    // Root hash is the single hash at the top level
    auto& top = levels.back();
    std::string root_hash = bytes_to_hex(top.data(), digest_size);

    return VerityResult{root_hash, salt_str, total_hash_size};
}

bool verify_verity_hash(const std::string& bundle_path,
                        uint64_t data_size,
                        const std::string& expected_root_hash,
                        const std::string& salt) {
    // Re-compute and compare root hash
    // Note: in production, the kernel dm-verity target handles on-the-fly verification.
    // This is for offline verification only.
    try {
        auto result = compute_verity_hash(bundle_path, data_size, salt);
        return result.root_hash == expected_root_hash;
    } catch (...) {
        return false;
    }
}

} // namespace aegis
