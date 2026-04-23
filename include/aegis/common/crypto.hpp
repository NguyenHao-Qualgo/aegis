#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <openssl/evp.h>

#include "aegis/core/types.hpp"

namespace aegis {

std::vector<unsigned char> hex_to_bytes(const std::string &hex);
const EVP_CIPHER          *evp_cipher_for_key_length(std::size_t key_len);
std::string                collect_openssl_errors();

EVP_PKEY *load_public_key_or_certificate(const std::string &path, std::string &detail);

bool verify_rsa_signature_openssl(const std::string &sw_description,
                                  const std::string &signature,
                                  const std::string &public_key,
                                  std::string &detail);

void verify_signature(const std::string &sw_description,
                      const std::string &signature,
                      const std::string &public_key);

}  // namespace aegis
