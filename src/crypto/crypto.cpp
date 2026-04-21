#include "aegis/crypto/crypto.hpp"

#include <memory>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

namespace aegis {

std::vector<unsigned char> hex_to_bytes(const std::string &hex) {
    if ((hex.size() % 2U) != 0U) { fail_runtime("hex string has odd length"); }
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2U);
    for (std::size_t i = 0; i < hex.size(); i += 2U) {
        auto nibble = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') { return static_cast<unsigned char>(c - '0'); }
            if (c >= 'a' && c <= 'f') { return static_cast<unsigned char>(10 + c - 'a'); }
            if (c >= 'A' && c <= 'F') { return static_cast<unsigned char>(10 + c - 'A'); }
            fail_runtime("invalid hex character");
        };
        out.push_back(static_cast<unsigned char>((nibble(hex[i]) << 4U) | nibble(hex[i + 1U])));
    }
    return out;
}

const EVP_CIPHER *evp_cipher_for_key_length(std::size_t key_len) {
    switch (key_len) {
    case 16: return EVP_aes_128_cbc();
    case 24: return EVP_aes_192_cbc();
    case 32: return EVP_aes_256_cbc();
    default: return nullptr;
    }
}

std::string collect_openssl_errors() {
    std::string out;
    unsigned long err = 0;
    char buffer[256] = {};
    while ((err = ERR_get_error()) != 0) {
        ERR_error_string_n(err, buffer, sizeof(buffer));
        if (!out.empty()) { out += '\n'; }
        out += buffer;
    }
    return out;
}

EVP_PKEY *load_public_key_or_certificate(const std::string &path, std::string &detail) {
    ERR_clear_error();
    using FilePtr = std::unique_ptr<FILE, int (*)(FILE *)>;
    FilePtr fp(::fopen(path.c_str(), "rb"), ::fclose);
    if (!fp) { detail = "cannot open key/certificate file: " + path; return nullptr; }

    EVP_PKEY *pkey = PEM_read_PUBKEY(fp.get(), nullptr, nullptr, nullptr);
    if (pkey != nullptr) { return pkey; }
    std::string pubkey_errors = collect_openssl_errors();

    if (::fseek(fp.get(), 0, SEEK_SET) != 0) {
        detail = "failed to rewind key/certificate file: " + path;
        return nullptr;
    }

    ERR_clear_error();
    X509 *cert = PEM_read_X509(fp.get(), nullptr, nullptr, nullptr);
    if (cert != nullptr) {
        pkey = X509_get_pubkey(cert);
        X509_free(cert);
        if (pkey != nullptr) { return pkey; }
        detail = "certificate file did not contain an extractable public key";
        const std::string cert_errors = collect_openssl_errors();
        if (!cert_errors.empty()) { detail += "\n" + cert_errors; }
        return nullptr;
    }

    detail = "file is neither a PEM public key nor a PEM certificate";
    const std::string cert_errors = collect_openssl_errors();
    if (!pubkey_errors.empty()) { detail += "\npublic key parse:\n" + pubkey_errors; }
    if (!cert_errors.empty())   { detail += "\ncertificate parse:\n" + cert_errors; }
    return nullptr;
}

bool verify_rsa_signature_openssl(const std::string &sw_description,
                                  const std::string &signature,
                                  const std::string &public_key,
                                  std::string &detail) {
    ERR_clear_error();
    EVP_PKEY *pkey = load_public_key_or_certificate(public_key, detail);
    if (pkey == nullptr) { return false; }

    using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    using PKeyPtr  = std::unique_ptr<EVP_PKEY,   decltype(&EVP_PKEY_free)>;
    PKeyPtr  key_guard(pkey, EVP_PKEY_free);
    MdCtxPtr ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
        detail = "EVP_MD_CTX_new failed";
        const std::string errors = collect_openssl_errors();
        if (!errors.empty()) { detail += "\n" + errors; }
        return false;
    }

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key_guard.get()) <= 0) {
        detail = "EVP_DigestVerifyInit failed";
        const std::string errors = collect_openssl_errors();
        if (!errors.empty()) { detail += "\n" + errors; }
        return false;
    }

    if (EVP_DigestVerifyUpdate(ctx.get(), sw_description.data(), sw_description.size()) <= 0) {
        detail = "EVP_DigestVerifyUpdate failed";
        const std::string errors = collect_openssl_errors();
        if (!errors.empty()) { detail += "\n" + errors; }
        return false;
    }

    const int rc = EVP_DigestVerifyFinal(
        ctx.get(),
        reinterpret_cast<const unsigned char *>(signature.data()),
        signature.size());
    if (rc == 1) { return true; }

    detail = (rc == 0) ? "signature mismatch" : "EVP_DigestVerifyFinal failed";
    const std::string errors = collect_openssl_errors();
    if (!errors.empty()) { detail += "\n" + errors; }
    return false;
}

void verify_signature(const std::string &sw_description,
                      const std::string &signature,
                      const std::string &public_key) {
    std::string detail;
    if (verify_rsa_signature_openssl(sw_description, signature, public_key, detail)) { return; }
    fail_runtime("sw-description signature verification failed via OpenSSL library\n" + detail);
}

}  // namespace aegis
