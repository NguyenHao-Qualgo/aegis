#pragma once

#include <cstdint>
#include <string>

namespace aegis {

/// Parameters for verity hash tree computation
struct VerityParams {
    std::string data_device; ///< path to data (squashfs payload)
    uint64_t data_size = 0;  ///< payload size in bytes
    uint32_t block_size = 4096;
    std::string salt; ///< hex-encoded salt
};

/// Result of verity hash computation
struct VerityResult {
    std::string root_hash; ///< hex-encoded root hash
    std::string salt;      ///< hex-encoded salt
    uint64_t hash_size;    ///< total hash tree size in bytes
};

/// Compute dm-verity hash tree and append it to the bundle file.
/// Returns the root hash, salt, and hash tree size.
VerityResult compute_verity_hash(const std::string& bundle_path, uint64_t data_size,
                                 const std::string& salt = {});

bool verify_verity_hash(const std::string& bundle_path, uint64_t data_size,
                        const std::string& expected_root_hash, const std::string& salt);

std::string generate_salt(size_t bytes = 32);

} // namespace aegis
