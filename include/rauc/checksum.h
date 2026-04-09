#pragma once

#include <cstdint>
#include <string>

namespace rauc {

/// Result of a checksum computation
struct Checksum {
    std::string digest;  ///< hex-encoded SHA-256
    uint64_t    size = 0;
};

/// Compute SHA-256 of a file
Checksum compute_checksum(const std::string& path);

/// Compute SHA-256 of in-memory data
std::string sha256_hex(const void* data, size_t len);

/// Verify a file's checksum against expected values
bool verify_checksum(const std::string& path,
                     const std::string& expected_digest,
                     uint64_t expected_size);

} // namespace rauc
