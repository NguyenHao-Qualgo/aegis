#include "aegis/verity_hash.h"
#include "aegis/utils.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

namespace aegis {
namespace {

constexpr size_t kDataBlockSize = 4096;
constexpr size_t kHashBlockSize = 4096;
constexpr size_t kDigestSize = 32; // sha256
constexpr size_t kSaltSize = 32;
constexpr int kVerityMaxLevels = 63;

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

std::string bytes_to_hex(const uint8_t* data, size_t len) {
    std::string hex;
    hex.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        char h[3];
        std::snprintf(h, sizeof(h), "%02x", data[i]);
        hex += h;
    }
    return hex;
}

unsigned get_bits_up(size_t u) {
    unsigned i = 0;
    while ((static_cast<size_t>(1) << i) < u) {
        ++i;
    }
    return i;
}

unsigned get_bits_down(size_t u) {
    unsigned i = 0;
    while ((u >> i) > 1U) {
        ++i;
    }
    return i;
}

bool mult_overflow_u64(uint64_t a, uint64_t b, uint64_t* out) {
    if (a == 0 || b == 0) {
        *out = 0;
        return false;
    }
    if (a > UINT64_MAX / b) {
        return true;
    }
    *out = a * b;
    return false;
}

void sha256_verity_v1(uint8_t* out_hash, const uint8_t* data, const uint8_t* salt) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        throw AegisError("EVP_MD_CTX_new failed");
    }

    unsigned int out_len = 0;
    const bool ok =
        EVP_DigestInit(mdctx, EVP_sha256()) == 1 &&
        EVP_DigestUpdate(mdctx, salt, kSaltSize) == 1 &&
        EVP_DigestUpdate(mdctx, data, kDataBlockSize) == 1 &&
        EVP_DigestFinal(mdctx, out_hash, &out_len) == 1;

    EVP_MD_CTX_free(mdctx);

    if (!ok || out_len != kDigestSize) {
        throw AegisError("Failed to compute dm-verity SHA256 digest");
    }
}

int hash_levels(uint64_t data_file_blocks,
                uint64_t* hash_position,
                int* levels,
                uint64_t* hash_level_block,
                uint64_t* hash_level_size) {
    if (kDigestSize == 0) {
        return -EINVAL;
    }

    const size_t hash_per_block_bits = get_bits_down(kHashBlockSize / kDigestSize);
    if (!hash_per_block_bits) {
        return -EINVAL;
    }

    *levels = 0;
    while (hash_per_block_bits * (*levels) < 64 &&
           ((data_file_blocks - 1) >> (hash_per_block_bits * (*levels)))) {
        ++(*levels);
    }

    if (*levels > kVerityMaxLevels) {
        return -EINVAL;
    }

    for (int i = *levels - 1; i >= 0; --i) {
        if (hash_level_block) {
            hash_level_block[i] = *hash_position;
        }

        const size_t s_shift = (i + 1) * hash_per_block_bits;
        if (s_shift > 63) {
            return -EINVAL;
        }

        const uint64_t s =
            (data_file_blocks + ((uint64_t)1 << s_shift) - 1) >> ((i + 1) * hash_per_block_bits);

        if (hash_level_size) {
            hash_level_size[i] = s;
        }

        if ((*hash_position + s) < *hash_position) {
            return -EINVAL;
        }
        *hash_position += s;
    }

    return 0;
}

int verify_zero(FILE* wr, size_t bytes) {
    std::vector<uint8_t> block(bytes);
    if (std::fread(block.data(), bytes, 1, wr) != 1) {
        return -EIO;
    }
    for (size_t i = 0; i < bytes; ++i) {
        if (block[i] != 0) {
            return -EPERM;
        }
    }
    return 0;
}

int create_or_verify(FILE* rd,
                     FILE* wr,
                     uint64_t data_block,
                     uint64_t hash_block,
                     uint64_t blocks,
                     bool verify,
                     uint8_t* calculated_digest,
                     const uint8_t* salt) {
    std::array<uint8_t, kHashBlockSize> left_block{};
    std::array<uint8_t, kDataBlockSize> data_buffer{};
    std::array<uint8_t, kDigestSize> read_digest{};

    const size_t hash_per_block = static_cast<size_t>(1) << get_bits_down(kHashBlockSize / kDigestSize);
    const size_t digest_size_full = static_cast<size_t>(1) << get_bits_up(kDigestSize);
    uint64_t blocks_to_write = (blocks + hash_per_block - 1) / hash_per_block;

    uint64_t seek_rd = 0;
    uint64_t seek_wr = 0;
    if (mult_overflow_u64(data_block, kDataBlockSize, &seek_rd) ||
        mult_overflow_u64(hash_block, kHashBlockSize, &seek_wr)) {
        return -EINVAL;
    }

    if (::fseeko(rd, static_cast<off_t>(seek_rd), SEEK_SET) != 0) {
        return -EIO;
    }
    if (wr && ::fseeko(wr, static_cast<off_t>(seek_wr), SEEK_SET) != 0) {
        return -EIO;
    }

    while (blocks_to_write--) {
        size_t left_bytes = kHashBlockSize;

        for (size_t i = 0; i < hash_per_block; ++i) {
            if (!blocks) {
                break;
            }
            --blocks;

            if (std::fread(data_buffer.data(), kDataBlockSize, 1, rd) != 1) {
                return -EIO;
            }

            sha256_verity_v1(calculated_digest, data_buffer.data(), salt);

            if (!wr) {
                break;
            }

            if (verify) {
                if (std::fread(read_digest.data(), kDigestSize, 1, wr) != 1) {
                    return -EIO;
                }
                if (std::memcmp(read_digest.data(), calculated_digest, kDigestSize) != 0) {
                    return -EPERM;
                }
            } else {
                if (std::fwrite(calculated_digest, kDigestSize, 1, wr) != 1) {
                    return -EIO;
                }
            }

            if (digest_size_full > kDigestSize) {
                const size_t spare = digest_size_full - kDigestSize;
                if (verify) {
                    int r = verify_zero(wr, spare);
                    if (r) {
                        return r;
                    }
                } else if (std::fwrite(left_block.data(), spare, 1, wr) != 1) {
                    return -EIO;
                }
            }

            left_bytes -= digest_size_full;
        }

        if (wr && left_bytes) {
            if (verify) {
                int r = verify_zero(wr, left_bytes);
                if (r) {
                    return r;
                }
            } else if (std::fwrite(left_block.data(), left_bytes, 1, wr) != 1) {
                return -EIO;
            }
        }
    }

    return 0;
}

int verity_create_or_verify_hash(bool verify,
                                 int fd,
                                 uint64_t data_blocks,
                                 uint64_t* combined_blocks,
                                 uint8_t* root_hash,
                                 const uint8_t* salt) {
    uint64_t hash_position = data_blocks;
    uint8_t calculated_digest[kDigestSize] = {};
    uint64_t hash_level_block[kVerityMaxLevels] = {};
    uint64_t hash_level_size[kVerityMaxLevels] = {};
    int levels = 0;

    int r = hash_levels(data_blocks, &hash_position, &levels,
                        hash_level_block, hash_level_size);
    if (r) {
        return r;
    }

    if (combined_blocks) {
        *combined_blocks = hash_position;
    }

    const std::string proc_path = "/proc/self/fd/" + std::to_string(fd);
    FILE* data_file = std::fopen(proc_path.c_str(), "r");
    if (!data_file) {
        return -EIO;
    }

    FILE* hash_file = std::fopen(proc_path.c_str(), verify ? "r" : "r+");
    if (!hash_file) {
        std::fclose(data_file);
        return -EIO;
    }

    try {
        for (int i = 0; i < levels; ++i) {
            if (i == 0) {
                r = create_or_verify(data_file, hash_file,
                                     0,
                                     hash_level_block[i],
                                     data_blocks,
                                     verify,
                                     calculated_digest,
                                     salt);
            } else {
                FILE* hash_file_2 = std::fopen(proc_path.c_str(), "r");
                if (!hash_file_2) {
                    std::fclose(hash_file);
                    std::fclose(data_file);
                    return -EIO;
                }

                r = create_or_verify(hash_file_2, hash_file,
                                     hash_level_block[i - 1],
                                     hash_level_block[i],
                                     hash_level_size[i - 1],
                                     verify,
                                     calculated_digest,
                                     salt);
                std::fclose(hash_file_2);
            }

            if (r) {
                std::fclose(hash_file);
                std::fclose(data_file);
                return r;
            }
        }

        if (levels) {
            r = create_or_verify(hash_file, nullptr,
                                 hash_level_block[levels - 1],
                                 0,
                                 1,
                                 verify,
                                 calculated_digest,
                                 salt);
        } else {
            r = create_or_verify(data_file, nullptr,
                                 0,
                                 0,
                                 data_blocks,
                                 verify,
                                 calculated_digest,
                                 salt);
        }

        std::fclose(hash_file);
        std::fclose(data_file);
        if (r) {
            return r;
        }

        if (verify) {
            return std::memcmp(root_hash, calculated_digest, kDigestSize) == 0 ? 0 : -EPERM;
        }

        std::memcpy(root_hash, calculated_digest, kDigestSize);
        return 0;
    } catch (...) {
        std::fclose(hash_file);
        std::fclose(data_file);
        throw;
    }
}

} // namespace

std::string generate_salt(size_t bytes) {
    std::vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), static_cast<int>(bytes)) != 1) {
        throw AegisError("RAND_bytes failed for salt generation");
    }

    std::string hex;
    hex.reserve(bytes * 2);
    for (auto b : buf) {
        char h[3];
        std::snprintf(h, sizeof(h), "%02x", b);
        hex += h;
    }
    return hex;
}

VerityResult compute_verity_hash(const std::string& bundle_path, uint64_t data_size,
                                 const std::string& salt_hex) {
    if (data_size == 0 || (data_size % kDataBlockSize) != 0) {
        throw AegisError("dm-verity data_size must be a non-zero multiple of 4096");
    }

    std::string salt_str = salt_hex.empty() ? generate_salt(kSaltSize) : salt_hex;
    auto salt = hex_to_bytes(salt_str);
    if (salt.size() != kSaltSize) {
        throw AegisError("dm-verity salt must be 32 bytes");
    }

    int fd = ::open(bundle_path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        throw AegisError("Cannot open bundle for verity: " + bundle_path + ": " +
                         std::string(std::strerror(errno)));
    }

    LOG_INFO("verity compute: bundle=%s data_size=%lu block_size=%zu",
             bundle_path.c_str(), data_size, kDataBlockSize);

    const uint64_t data_blocks = data_size / kDataBlockSize;
    uint64_t combined_blocks = 0;
    uint8_t root_hash_bin[kDigestSize] = {};

    const int r = verity_create_or_verify_hash(false, fd, data_blocks, &combined_blocks,
                                               root_hash_bin, salt.data());
    ::close(fd);

    if (r != 0) {
        throw AegisError("Failed to create dm-verity hash tree: error " + std::to_string(r));
    }

    const uint64_t hash_blocks = combined_blocks - data_blocks;
    const uint64_t hash_size = hash_blocks * kHashBlockSize;
    std::string root_hash = bytes_to_hex(root_hash_bin, kDigestSize);

    LOG_INFO("verity compute: data_blocks=%lu combined_blocks=%lu hash_blocks=%lu hash_size=%lu",
             data_blocks, combined_blocks, hash_blocks, hash_size);
    LOG_INFO("verity compute: root_hash=%s", root_hash.c_str());

    return VerityResult{root_hash, salt_str, hash_size};
}

bool verify_verity_hash(const std::string& bundle_path, uint64_t data_size,
                        const std::string& expected_root_hash, const std::string& salt_hex) {
    if (data_size == 0 || (data_size % kDataBlockSize) != 0) {
        return false;
    }

    auto salt = hex_to_bytes(salt_hex);
    auto root_hash = hex_to_bytes(expected_root_hash);
    if (salt.size() != kSaltSize || root_hash.size() != kDigestSize) {
        return false;
    }

    int fd = ::open(bundle_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    const uint64_t data_blocks = data_size / kDataBlockSize;
    const int r = verity_create_or_verify_hash(true, fd, data_blocks, nullptr,
                                               root_hash.data(), salt.data());
    ::close(fd);
    return r == 0;
}

} // namespace aegis
