#include "aegis/bundle/bundle_verifier.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <openssl/bio.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "aegis/util.hpp"

namespace aegis {

namespace {

using BioPtr      = std::unique_ptr<BIO,              decltype(&BIO_free)>;
using CmsPtr      = std::unique_ptr<CMS_ContentInfo,  decltype(&CMS_ContentInfo_free)>;
using StorePtr    = std::unique_ptr<X509_STORE,        decltype(&X509_STORE_free)>;
using EvpCtxPtr   = std::unique_ptr<EVP_MD_CTX,        decltype(&EVP_MD_CTX_free)>;

std::string opensslError() {
    std::string result;
    char buf[256];
    unsigned long code;
    while ((code = ERR_get_error()) != 0) {
        ERR_error_string_n(code, buf, sizeof(buf));
        if (!result.empty()) result += "; ";
        result += buf;
    }
    return result.empty() ? "unknown error" : result;
}

}  // namespace

std::string BundleVerifier::sha256(const std::string& path) const {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for hashing: " + path);
    }

    EvpCtxPtr ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("EVP_DigestInit_ex failed: " + opensslError());
    }

    char buf[8192];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx.get(), buf, static_cast<std::size_t>(file.gcount())) != 1) {
            throw std::runtime_error("EVP_DigestUpdate failed: " + opensslError());
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &digestLen) != 1) {
        throw std::runtime_error("EVP_DigestFinal_ex failed: " + opensslError());
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLen; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::optional<BundleVerifier::SignatureInfo> BundleVerifier::signatureInfo(const std::string& bundlePath) const {
    const auto bundleSize = std::filesystem::file_size(bundlePath);
    if (bundleSize < 8) {
        return std::nullopt;
    }

    std::ifstream input(bundlePath, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open bundle: " + bundlePath);
    }

    input.seekg(-8, std::ios::end);
    unsigned char trailer[8];
    input.read(reinterpret_cast<char*>(trailer), sizeof(trailer));
    if (!input) {
        throw std::runtime_error("Cannot read bundle signature trailer: " + bundlePath);
    }

    std::uint64_t cmsSize = 0;
    for (int i = 0; i < 8; ++i) {
        cmsSize |= static_cast<std::uint64_t>(trailer[i]) << (i * 8);
    }

    if (cmsSize == 0 || cmsSize > bundleSize - 8) {
        return std::nullopt;
    }

    SignatureInfo info;
    info.cmsSize = cmsSize;
    info.cmsOffset = bundleSize - 8 - cmsSize;
    info.payloadSize = info.cmsOffset;
    if (info.payloadSize == 0) {
        return std::nullopt;
    }
    return info;
}

std::uint64_t BundleVerifier::payloadSize(const std::string& bundlePath) const {
    const auto info = signatureInfo(bundlePath);
    return info ? info->payloadSize : std::filesystem::file_size(bundlePath);
}

std::optional<BundleManifest> BundleVerifier::verifyBundle(const std::string& bundlePath, const OtaConfig& config) const {
    const auto info = signatureInfo(bundlePath);
    if (!info) {
        if (!config.keyringPath.empty()) {
            throw std::runtime_error("Unsigned bundle rejected because keyring.path is configured");
        }
        return std::nullopt;
    }

    if (config.keyringPath.empty()) {
        throw std::runtime_error("Signed bundle verification requires keyring.path in config");
    }

    // Extract CMS bytes from the bundle into memory
    std::vector<unsigned char> cmsData(info->cmsSize);
    {
        std::ifstream input(bundlePath, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot open bundle: " + bundlePath);
        }
        input.seekg(static_cast<std::streamoff>(info->cmsOffset), std::ios::beg);
        input.read(reinterpret_cast<char*>(cmsData.data()), static_cast<std::streamsize>(info->cmsSize));
        if (static_cast<std::uint64_t>(input.gcount()) != info->cmsSize) {
            throw std::runtime_error("Failed to extract CMS from bundle");
        }
    }

    // Build trust store from keyring
    StorePtr store(X509_STORE_new(), X509_STORE_free);
    if (!store) {
        throw std::runtime_error("X509_STORE_new failed");
    }
    if (X509_STORE_load_locations(store.get(), config.keyringPath.c_str(), nullptr) != 1) {
        throw std::runtime_error("Failed to load keyring '" + config.keyringPath + "': " + opensslError());
    }

    // Parse DER-encoded CMS from memory
    BioPtr cmsBio(BIO_new_mem_buf(cmsData.data(), static_cast<int>(cmsData.size())), BIO_free);
    if (!cmsBio) {
        throw std::runtime_error("BIO_new_mem_buf failed");
    }
    CmsPtr cms(d2i_CMS_bio(cmsBio.get(), nullptr), CMS_ContentInfo_free);
    if (!cms) {
        throw std::runtime_error("Failed to parse CMS structure: " + opensslError());
    }

    // Verify signature and extract embedded manifest content into a memory BIO
    BioPtr contentBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!contentBio) {
        throw std::runtime_error("BIO_new failed");
    }
    ERR_clear_error();
    if (CMS_verify(cms.get(), nullptr, store.get(), nullptr, contentBio.get(), CMS_BINARY) != 1) {
        throw std::runtime_error("Bundle signature verification failed: " + opensslError());
    }

    // Read manifest content from memory BIO
    char* contentPtr = nullptr;
    const long contentLen = BIO_get_mem_data(contentBio.get(), &contentPtr);
    if (contentLen <= 0) {
        throw std::runtime_error("CMS verification produced empty manifest");
    }

    // Write manifest to a work dir inside the daemon's data directory (not /tmp)
    const auto workDir = std::filesystem::path(config.dataDirectory) / "bundle-verify";
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);
    const auto manifestPath = workDir / "manifest.ini";

    {
        std::ofstream manifestOut(manifestPath, std::ios::binary);
        if (!manifestOut) {
            std::filesystem::remove_all(workDir);
            throw std::runtime_error("Cannot write extracted manifest");
        }
        manifestOut.write(contentPtr, contentLen);
    }

    BundleManifestIO io;
    const auto manifest = io.loadFromFile(manifestPath.string());
    std::filesystem::remove_all(workDir);

    if (manifest.compatible != config.compatible) {
        throw std::runtime_error("Bundle compatible mismatch");
    }
    logInfo("CMS signature verified  compatible=" + manifest.compatible +
            "  version=" + manifest.version);
    return manifest;
}

BundleManifest BundleVerifier::loadManifest(const std::string& extractedDir, const OtaConfig& config) const {
    BundleManifestIO io;
    const auto manifest = io.loadFromFile(joinPath(extractedDir, "manifest.ini"));
    if (manifest.compatible != config.compatible) {
        throw std::runtime_error("Bundle compatible mismatch");
    }
    return manifest;
}

std::string BundleVerifier::resolvePayloadPath(const std::string& extractedDir, const std::string& filename) const {
    const auto relativePath = std::filesystem::path(filename).lexically_normal();
    if (relativePath.empty() || relativePath.is_absolute()) {
        throw std::runtime_error("Invalid bundle payload path: " + filename);
    }
    for (const auto& part : relativePath) {
        if (part == "..") {
            throw std::runtime_error("Bundle payload path escapes bundle directory: " + filename);
        }
    }
    return (std::filesystem::path(extractedDir) / relativePath).string();
}

void BundleVerifier::verifyPayloads(const BundleManifest& manifest, const std::string& extractedDir) const {
    for (const auto& image : manifest.images) {
        const auto payloadPath = resolvePayloadPath(extractedDir, image.filename);
        if (!std::filesystem::exists(payloadPath)) {
            throw std::runtime_error("Missing payload file: " + image.filename);
        }
        if (std::filesystem::file_size(payloadPath) != image.size) {
            throw std::runtime_error("Payload size mismatch: " + image.filename);
        }
        if (sha256(payloadPath) != image.sha256) {
            throw std::runtime_error("Payload checksum mismatch: " + image.filename);
        }
    }
}

}  // namespace aegis
