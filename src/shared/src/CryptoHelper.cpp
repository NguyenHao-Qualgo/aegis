
#include "CryptoHelper.h"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/provider.h>
#include <openssl/store.h>
#include <openssl/x509.h>
#include <iostream>
#include <memory>
#include "env.h"
#include "utils.h"

namespace {
class CryptoProviderManager {
public:
    CryptoProviderManager() = default;
    ~CryptoProviderManager() { Cleanup(); }

    CryptoProviderManager(const CryptoProviderManager&) = delete;
    CryptoProviderManager& operator=(const CryptoProviderManager&) = delete;

    CryptoProviderManager(CryptoProviderManager&& other) noexcept
        : openssl_ctx_(other.openssl_ctx_),
          nxp_provider_(other.nxp_provider_),
          default_provider_(other.default_provider_) {
        other.openssl_ctx_ = nullptr;
        other.nxp_provider_ = nullptr;
        other.default_provider_ = nullptr;
    }

    void Init() {
        if (!openssl_ctx_) {
            openssl_ctx_ = OSSL_LIB_CTX_new();
            if (!openssl_ctx_) {
                LOG_E("Failed to create OpenSSL libctx");
                return;
            }
        }
        if (!nxp_provider_) {
            nxp_provider_ = OSSL_PROVIDER_load(openssl_ctx_, PROVIDER_SO_PATH);
            if (!nxp_provider_) {
                LOG_E("Failed to load NXP provider");
            }
        }
        if (!default_provider_) {
            default_provider_ = OSSL_PROVIDER_load(openssl_ctx_, "default");
            if (!default_provider_) {
                LOG_E("Failed to load default provider");
            }
        }
    }

    void Cleanup() {
        if (default_provider_) {
            OSSL_PROVIDER_unload(default_provider_);
            default_provider_ = nullptr;
        }
        if (nxp_provider_) {
            OSSL_PROVIDER_unload(nxp_provider_);
            nxp_provider_ = nullptr;
        }
        if (openssl_ctx_) {
            OSSL_LIB_CTX_free(openssl_ctx_);
            openssl_ctx_ = nullptr;
        }
    }

    OSSL_LIB_CTX* Ctx() const { return openssl_ctx_; }
    bool IsReady() const { return openssl_ctx_ && nxp_provider_ && default_provider_; }
    bool HasNXP() const { return nxp_provider_ != nullptr; }

private:
    OSSL_LIB_CTX* openssl_ctx_{nullptr};
    OSSL_PROVIDER* nxp_provider_{nullptr};
    OSSL_PROVIDER* default_provider_{nullptr};
};

const std::unique_ptr<CryptoProviderManager> g_crypto_manager = std::make_unique<CryptoProviderManager>();
}

namespace CryptoHelper {

namespace {

void set_cert_permissions(const std::filesystem::path& cert, const std::filesystem::path& key) {
    std::filesystem::perms options =
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::group_read;
    std::error_code ec;
    if (std::filesystem::exists(cert, ec)) {
        std::filesystem::permissions(cert, options, ec);
    }
    if (std::filesystem::exists(key, ec)) {
        std::filesystem::permissions(key, options, ec);
    }
}

bool write_cert_and_key(const X509* cert, const EVP_PKEY* pkey, const std::filesystem::path& cert_path,
    const std::filesystem::path& key_path) {
    std::unique_ptr<BIO, decltype(&BIO_free_all)> certFile{BIO_new_file(cert_path.c_str(), "wb"), &BIO_free_all};
    std::unique_ptr<BIO, decltype(&BIO_free_all)> keyFile{BIO_new_file(key_path.c_str(), "wb"), &BIO_free_all};

    if (!keyFile ||
        !PEM_write_bio_PrivateKey(keyFile.get(), const_cast<EVP_PKEY*>(pkey), nullptr, nullptr, 0, nullptr, nullptr)) {
        LOG_E("Failed to write private key to {}", key_path.string());
        ERR_print_errors_fp(stderr);
        return false;
    }
    if (!certFile || !PEM_write_bio_X509(certFile.get(), const_cast<X509*>(cert))) {
        LOG_E("Failed to write certificate to {}", cert_path.string());
        ERR_print_errors_fp(stderr);
        return false;
    }
    set_cert_permissions(cert_path, key_path);
    return true;
}

bool create_x509_certificate(EVP_PKEY* pkey, X509*& cert, const std::string& common_name) {
    cert = X509_new();
    if (!cert)
        return false;
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 315360000L);  // 10 years

    X509_NAME* name = X509_get_subject_name(cert);
    if (!name)
        return false;
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (const unsigned char*)"US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (const unsigned char*)"CA", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (const unsigned char*)"MotorolaSolutions", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC, (const unsigned char*)"CameraL6D", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)common_name.c_str(), -1, -1, 0);

    X509_set_issuer_name(cert, name);
    X509_set_pubkey(cert, pkey);

    if (!X509_sign(cert, pkey, EVP_sha256())) {
        X509_free(cert);
        cert = nullptr;
        return false;
    }
    return true;
}

}  // namespace

bool SelfSignCertificate(OSSL_LIB_CTX*& ctx, EVP_PKEY_CTX*& gen_ctx, EVP_PKEY*& pkey, X509*& cert,
    const std::filesystem::path& ssl_certificate, const std::filesystem::path& ssl_certificate_key, const std::string& common_name) {
    ctx = OSSL_LIB_CTX_new();
    if (!ctx)
        return false;
    gen_ctx = EVP_PKEY_CTX_new_from_name(ctx, "RSA", nullptr);
    if (!gen_ctx)
        return false;
    if (EVP_PKEY_keygen_init(gen_ctx) <= 0)
        return false;
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(gen_ctx, 4096) <= 0)
        return false;
    pkey = nullptr;
    if (EVP_PKEY_keygen(gen_ctx, &pkey) <= 0 || !pkey)
        return false;
    if (!create_x509_certificate(pkey, cert, common_name))
        return false;
    return write_cert_and_key(cert, pkey, ssl_certificate, ssl_certificate_key);
}

bool genNXPCert(const std::string& common_name) {
    g_crypto_manager->Init();
    if (!g_crypto_manager->IsReady()) {
        LOG_E("OpenSSL context/provider not initialized");
        return false;
    }
    OSSL_STORE_CTX* store_ctx =
        OSSL_STORE_open_ex(KEY_REF, g_crypto_manager->Ctx(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!store_ctx) {
        LOG_E("Failed to open store for: {}", KEY_REF);
        ERR_print_errors_fp(stderr);
        return false;
    }
    EVP_PKEY* pkey = nullptr;
    while (!OSSL_STORE_eof(store_ctx) && !pkey) {
        OSSL_STORE_INFO* info = OSSL_STORE_load(store_ctx);
        if (!info)
            break;
        if (OSSL_STORE_INFO_get_type(info) == OSSL_STORE_INFO_PKEY) {
            pkey = OSSL_STORE_INFO_get1_PKEY(info);
        }
        OSSL_STORE_INFO_free(info);
    }
    OSSL_STORE_close(store_ctx);
    if (!pkey) {
        LOG_E("Failed to load private key from provider");
        ERR_print_errors_fp(stderr);
        return false;
    }

    X509* cert = nullptr;
    if (!create_x509_certificate(pkey, cert, common_name)) {
        EVP_PKEY_free(pkey);
        return false;
    }

    bool ok = write_cert_and_key(cert, pkey, Env::_ssl_certificate, Env::_ssl_priv_key);

    X509_free(cert);
    EVP_PKEY_free(pkey);
    return ok;
}

bool GenerateCertificate(bool& use_nxp) {
    const std::filesystem::path& default_cert = Env::_ssl_certificate;
    const std::filesystem::path& default_key = Env::_ssl_priv_key;
    LOG_D("_ssl_certificate: {}", default_cert.string());
    LOG_D("_ssl_priv_key: {}", default_key.string());
    std::error_code ec;
    if (std::filesystem::exists(default_cert, ec))
        std::filesystem::remove(default_cert, ec);
    if (std::filesystem::exists(default_key, ec))
        std::filesystem::remove(default_key, ec);

    bool result = false;
    std::string common_name;
    if (std::string(CAMERA_TYPE) == "L6D") {
        common_name = GenerateUUIDFromString(getMacAddress());
        result = genNXPCert(common_name);
        if (result) {
            use_nxp = true;
            return true;
        }
        LOG_W("Could not generate NXP certificate and key file, fall back to default openssl");
    } else {
        // L6Q
        common_name = GenerateUUIDFromString(ReadFileContent("/mnt/calib/customer-serial"));
    }
    OSSL_LIB_CTX* ctx = nullptr;
    EVP_PKEY_CTX* gen_ctx = nullptr;
    EVP_PKEY* pkey = nullptr;
    X509* cert = nullptr;
    result = SelfSignCertificate(ctx, gen_ctx, pkey, cert, default_cert, default_key, common_name);
    if (!result) {
        LOG_W("Could not generate default certificate and key file.");
    }
    if (pkey)
        EVP_PKEY_free(pkey);
    if (cert)
        X509_free(cert);
    if (gen_ctx)
        EVP_PKEY_CTX_free(gen_ctx);
    if (ctx)
        OSSL_LIB_CTX_free(ctx);
    return result;
}

bool HasNXP() {
    g_crypto_manager->Init();
    return g_crypto_manager->HasNXP();
}

void CleanUpProvider() {
    g_crypto_manager->Init();
    g_crypto_manager->Cleanup();
    unsetenv("OPENSSL_CONF");
}
}  // namespace CryptoHelper