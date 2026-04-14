#include "aegis/signature.h"
#include "aegis/utils.h"

#include <cstdio>
#include <openssl/bio.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <memory>

namespace aegis {

static std::string openssl_error_string() {
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    return std::string(buf);
}

Bytes read_file_bytes(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) throw SignatureError("Cannot open: " + path);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    Bytes data(sz);
    if (fread(data.data(), 1, sz, f) != static_cast<size_t>(sz)) {
        fclose(f);
        throw SignatureError("Read error: " + path);
    }
    fclose(f);
    return data;
}

void write_file_bytes(const std::string& path, const Bytes& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) throw SignatureError("Cannot write: " + path);
    if (fwrite(data.data(), 1, data.size(), f) != data.size()) {
        fclose(f);
        throw SignatureError("Write error: " + path);
    }
    fclose(f);
}

// RAII wrapper for OpenSSL BIO
struct BioDeleter { void operator()(BIO* b) const { BIO_free_all(b); } };
using UniqueBio = std::unique_ptr<BIO, BioDeleter>;

struct CmsDeleter { void operator()(CMS_ContentInfo* c) const { CMS_ContentInfo_free(c); } };
using UniqueCms = std::unique_ptr<CMS_ContentInfo, CmsDeleter>;

struct X509Deleter { void operator()(X509* x) const { X509_free(x); } };
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

struct PkeyDeleter { void operator()(EVP_PKEY* k) const { EVP_PKEY_free(k); } };
using UniquePkey = std::unique_ptr<EVP_PKEY, PkeyDeleter>;

struct StoreDeleter { void operator()(X509_STORE* s) const { X509_STORE_free(s); } };
using UniqueStore = std::unique_ptr<X509_STORE, StoreDeleter>;

static UniqueX509 load_cert(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) throw SignatureError("Cannot open cert: " + path);
    X509* cert = PEM_read_X509(f, nullptr, nullptr, nullptr);
    fclose(f);
    if (!cert) throw SignatureError("Failed to read cert: " + path);
    return UniqueX509(cert);
}

static UniquePkey load_key(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) throw SignatureError("Cannot open key: " + path);
    EVP_PKEY* key = PEM_read_PrivateKey(f, nullptr, nullptr, nullptr);
    fclose(f);
    if (!key) throw SignatureError("Failed to read key: " + path);
    return UniquePkey(key);
}

static UniqueStore build_trust_store(const SigningParams& params) {
    UniqueStore store(X509_STORE_new());
    if (!store) throw SignatureError("X509_STORE_new failed");

    if (!params.keyring_path.empty()) {
        if (X509_STORE_load_locations(store.get(), params.keyring_path.c_str(), nullptr) != 1)
            throw SignatureError("Failed to load keyring: " + params.keyring_path);
    }

    if (params.check_crl) {
        X509_STORE_set_flags(store.get(), X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    }
    if (params.allow_partial_chain) {
        X509_STORE_set_flags(store.get(), X509_V_FLAG_PARTIAL_CHAIN);
    }
    if (params.no_check_time) {
        X509_STORE_set_flags(store.get(), X509_V_FLAG_NO_CHECK_TIME);
    }

    return store;
}

Bytes cms_sign(const Bytes& data, const SigningParams& params) {
    auto cert = load_cert(params.cert_path);
    auto key  = load_key(params.key_path);

    UniqueBio in(BIO_new_mem_buf(data.data(), static_cast<int>(data.size())));
    if (!in) throw SignatureError("BIO_new_mem_buf failed");

    int flags = CMS_BINARY;
    CMS_ContentInfo* cms = CMS_sign(cert.get(), key.get(), nullptr, in.get(), flags);
    if (!cms) throw SignatureError("CMS_sign failed: " + openssl_error_string());
    UniqueCms cms_guard(cms);

    UniqueBio out(BIO_new(BIO_s_mem()));
    if (i2d_CMS_bio(out.get(), cms) != 1)
        throw SignatureError("i2d_CMS_bio failed");

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(out.get(), &bptr);
    return Bytes(bptr->data, bptr->data + bptr->length);
}

Bytes cms_sign_file(const std::string& path, uint64_t size,
                    const SigningParams& params) {
    auto cert = load_cert(params.cert_path);
    auto key  = load_key(params.key_path);

    UniqueBio in(BIO_new_file(path.c_str(), "rb"));
    if (!in) throw SignatureError("Cannot open for signing: " + path);

    int flags = CMS_BINARY | CMS_DETACHED;
    CMS_ContentInfo* cms = CMS_sign(cert.get(), key.get(), nullptr, in.get(), flags);
    if (!cms) throw SignatureError("CMS_sign_file failed: " + openssl_error_string());
    UniqueCms cms_guard(cms);

    UniqueBio out(BIO_new(BIO_s_mem()));
    if (i2d_CMS_bio(out.get(), cms) != 1)
        throw SignatureError("i2d_CMS_bio failed");

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(out.get(), &bptr);
    return Bytes(bptr->data, bptr->data + bptr->length);
}

Bytes cms_verify(const Bytes& cms_data, const SigningParams& params) {
    auto store = build_trust_store(params);

    UniqueBio cms_bio(BIO_new_mem_buf(cms_data.data(), static_cast<int>(cms_data.size())));
    CMS_ContentInfo* cms = d2i_CMS_bio(cms_bio.get(), nullptr);
    if (!cms) throw SignatureError("d2i_CMS_bio failed: " + openssl_error_string());
    UniqueCms cms_guard(cms);

    UniqueBio out(BIO_new(BIO_s_mem()));

    // For verity/crypt, the manifest is embedded in the CMS
    if (CMS_verify(cms, nullptr, store.get(), nullptr, out.get(), 0) != 1)
        throw SignatureError("CMS verification failed: " + openssl_error_string());

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(out.get(), &bptr);
    return Bytes(bptr->data, bptr->data + bptr->length);
}

Result<void> cms_verify_content(const Bytes& cms_data, const Bytes& content,
                                const SigningParams& params) {
    auto store = build_trust_store(params);

    UniqueBio cms_bio(BIO_new_mem_buf(cms_data.data(), static_cast<int>(cms_data.size())));
    CMS_ContentInfo* cms = d2i_CMS_bio(cms_bio.get(), nullptr);
    if (!cms) return Result<void>::err("d2i_CMS_bio failed");
    UniqueCms cms_guard(cms);

    UniqueBio content_bio(BIO_new_mem_buf(content.data(), static_cast<int>(content.size())));

    if (CMS_verify(cms, nullptr, store.get(), content_bio.get(), nullptr,
                   CMS_BINARY | CMS_DETACHED) != 1) {
        return Result<void>::err("CMS content verification failed: " + openssl_error_string());
    }
    return Result<void>::ok();
}

Bytes cms_encrypt(const Bytes& data, const std::vector<std::string>& recipient_certs) {
    STACK_OF(X509)* certs = sk_X509_new_null();
    std::vector<UniqueX509> cert_holders;
    for (auto& path : recipient_certs) {
        auto c = load_cert(path);
        sk_X509_push(certs, c.get());
        cert_holders.push_back(std::move(c));
    }

    UniqueBio in(BIO_new_mem_buf(data.data(), static_cast<int>(data.size())));
    CMS_ContentInfo* cms = CMS_encrypt(certs, in.get(), EVP_aes_256_cbc(),
                                       CMS_BINARY | CMS_STREAM);
    sk_X509_free(certs);
    if (!cms) throw SignatureError("CMS_encrypt failed: " + openssl_error_string());
    UniqueCms cms_guard(cms);

    UniqueBio out(BIO_new(BIO_s_mem()));
    if (i2d_CMS_bio(out.get(), cms) != 1)
        throw SignatureError("i2d_CMS_bio encrypt failed");

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(out.get(), &bptr);
    return Bytes(bptr->data, bptr->data + bptr->length);
}

Bytes cms_decrypt(const Bytes& data, const std::string& key_path,
                  const std::string& cert_path) {
    auto cert = load_cert(cert_path);
    auto key  = load_key(key_path);

    UniqueBio in(BIO_new_mem_buf(data.data(), static_cast<int>(data.size())));
    CMS_ContentInfo* cms = d2i_CMS_bio(in.get(), nullptr);
    if (!cms) throw SignatureError("d2i_CMS_bio decrypt failed");
    UniqueCms cms_guard(cms);

    UniqueBio out(BIO_new(BIO_s_mem()));
    if (CMS_decrypt(cms, key.get(), cert.get(), nullptr, out.get(), 0) != 1)
        throw SignatureError("CMS_decrypt failed: " + openssl_error_string());

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(out.get(), &bptr);
    return Bytes(bptr->data, bptr->data + bptr->length);
}

} // namespace aegis
