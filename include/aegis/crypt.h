#pragma once

#include "aegis/error.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aegis {

/// Encrypt a bundle payload using AES-256-CBC (dm-crypt compatible)
/// Writes encrypted data to output_path.
/// Returns the randomly generated AES key (hex-encoded).
std::string crypt_encrypt(const std::string& input_path, const std::string& output_path,
                          uint64_t data_size);

/// Decrypt a bundle payload using the given AES key
Result<void> crypt_decrypt(const std::string& input_path, const std::string& output_path,
                           const std::string& hex_key, uint64_t data_size);

/// Generate a random AES-256 key (hex-encoded, 64 chars)
std::string generate_aes_key();

} // namespace aegis
