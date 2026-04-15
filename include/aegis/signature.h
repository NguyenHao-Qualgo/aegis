#pragma once

#include "aegis/error.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aegis {

/// Opaque bytes wrapper
using Bytes = std::vector<uint8_t>;

/// CMS signing options
struct SigningParams {
    std::string cert_path;
    std::string key_path;
    std::string keyring_path; ///< trust anchors for verification
    bool check_crl = false;
    bool allow_partial_chain = false;
    bool no_check_time = false;
};

/// Sign data using CMS (PKCS#7)
Bytes cms_sign(const Bytes& data, const SigningParams& params);

/// Sign data from a file (memory-mapped for large bundles)
Bytes cms_sign_file(const std::string& path, uint64_t size, const SigningParams& params);

/// Verify CMS signature and return the signed content (manifest)
Bytes cms_verify(const Bytes& cms_data, const SigningParams& params);

/// Verify CMS signature against external content
Result<void> cms_verify_content(const Bytes& cms_data, const Bytes& content,
                                const SigningParams& params);

/// Encrypt CMS data to a list of recipient certificates
Bytes cms_encrypt(const Bytes& data, const std::vector<std::string>& recipient_certs);

/// Decrypt CMS data using a private key
Bytes cms_decrypt(const Bytes& data, const std::string& key_path, const std::string& cert_path);

/// Read a PEM/DER file into bytes
Bytes read_file_bytes(const std::string& path);

/// Write bytes to a file
void write_file_bytes(const std::string& path, const Bytes& data);

} // namespace aegis
