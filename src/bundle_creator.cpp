#include "aegis/bundle_creator.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

#include <openssl/bio.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "aegis/bundle_manifest.hpp"
#include "aegis/util.hpp"

namespace aegis {

namespace {

using BioPtr     = std::unique_ptr<BIO,             decltype(&BIO_free)>;
using CmsPtr     = std::unique_ptr<CMS_ContentInfo, decltype(&CMS_ContentInfo_free)>;
using X509Ptr    = std::unique_ptr<X509,            decltype(&X509_free)>;
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY,        decltype(&EVP_PKEY_free)>;
using EvpCtxPtr  = std::unique_ptr<EVP_MD_CTX,      decltype(&EVP_MD_CTX_free)>;

std::string opensslError() {
    char buf[256] = {};
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    return buf;
}

}  // namespace

BundleCreator::BundleCreator(CommandRunner runner) : runner_(std::move(runner)) {}

std::string BundleCreator::sha256(const std::string& path) const {
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

BundleArtifactInput BundleCreator::parseArtifactSpec(const std::string& spec) {
    const auto first = spec.find(':');
    const auto second = first == std::string::npos ? std::string::npos : spec.find(':', first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        throw std::runtime_error("Invalid --artifact spec: expected <slot-class>:<type>:<path>");
    }

    BundleArtifactInput input;
    input.slotClass = trim(spec.substr(0, first));
    input.imagetype = trim(spec.substr(first + 1, second - first - 1));
    input.path = trim(spec.substr(second + 1));
    if (input.slotClass.empty() || input.imagetype.empty() || input.path.empty()) {
        throw std::runtime_error("Invalid --artifact spec: empty field");
    }
    input.bundleFilename = std::filesystem::path(input.path).filename().string();
    if (input.bundleFilename.empty()) {
        throw std::runtime_error("Artifact path has no filename: " + input.path);
    }
    return input;
}

std::filesystem::path BundleCreator::validateRelativeBundlePath(const std::string& filename) {
    const auto path = std::filesystem::path(filename).lexically_normal();
    if (path.empty() || path.is_absolute()) {
        throw std::runtime_error("Bundle payload path must be a relative path: " + filename);
    }

    for (const auto& part : path) {
        if (part == "..") {
            throw std::runtime_error("Bundle payload path must stay inside the bundle: " + filename);
        }
    }

    return path;
}

BundleManifest BundleCreator::loadManifestTemplate(const BundleCreateOptions& options) const {
    if (options.manifestPath.empty()) {
        throw std::runtime_error("bundle create manifest flow requires --manifest");
    }

    BundleManifestIO io;
    auto manifest = io.loadFromFile(options.manifestPath, BundleManifestValidationMode::AllowMissingPayloadMetadata);
    if (manifest.format.empty()) {
        manifest.format = "plain";
    }
    return manifest;
}

BundleManifest BundleCreator::buildManifestFromArtifacts(const BundleCreateOptions& options) const {
    if (options.compatible.empty() || options.version.empty()) {
        throw std::runtime_error("bundle create requires compatible and version");
    }
    if (options.artifacts.empty()) {
        throw std::runtime_error("bundle create requires at least one --artifact");
    }

    BundleManifest manifest;
    manifest.version = options.version;
    manifest.compatible = options.compatible;
    manifest.format = options.format;

    for (const auto& artifact : options.artifacts) {
        BundleImage image;
        image.name = artifact.slotClass;
        image.slotClass = artifact.slotClass;
        image.sourceType = artifact.sourceType;
        image.imagetype = artifact.imagetype;
        image.filename = artifact.bundleFilename;
        manifest.images.push_back(std::move(image));
    }
    return manifest;
}

void BundleCreator::stageManifestPayloads(BundleManifest& manifest, const std::filesystem::path& sourceDir,
                                          const std::filesystem::path& workDir) const {
    for (auto& image : manifest.images) {
        const auto relativePath = validateRelativeBundlePath(image.filename);
        const auto sourcePath = sourceDir / relativePath;
        if (!std::filesystem::exists(sourcePath)) {
            throw std::runtime_error("Manifest payload does not exist: " + sourcePath.string());
        }

        const auto destPath = workDir / relativePath;
        std::filesystem::create_directories(destPath.parent_path());
        std::filesystem::copy_file(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing);

        image.sha256 = sha256(destPath.string());
        image.size = std::filesystem::file_size(destPath);
    }
}

void BundleCreator::signBundle(const std::filesystem::path& manifestPath, const std::filesystem::path& unsignedBundlePath,
                               const std::filesystem::path& outputBundlePath, const BundleCreateOptions& options) const {
    if (options.certPath.empty() || options.keyPath.empty()) {
        throw std::runtime_error("bundle create signing requires both --cert and --key");
    }

    const auto cmsPath = unsignedBundlePath.parent_path() / "manifest.cms";
    {
        BioPtr certBio(BIO_new_file(options.certPath.c_str(), "r"), BIO_free);
        if (!certBio) {
            throw std::runtime_error("Cannot open signing cert: " + options.certPath);
        }
        X509Ptr signerCert(PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr), X509_free);
        if (!signerCert) {
            throw std::runtime_error("Failed to read signing cert: " + opensslError());
        }

        BioPtr keyBio(BIO_new_file(options.keyPath.c_str(), "r"), BIO_free);
        if (!keyBio) {
            throw std::runtime_error("Cannot open signing key: " + options.keyPath);
        }
        EvpPkeyPtr signerKey(PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
        if (!signerKey) {
            throw std::runtime_error("Failed to read signing key: " + opensslError());
        }

        BioPtr inBio(BIO_new_file(manifestPath.string().c_str(), "rb"), BIO_free);
        if (!inBio) {
            throw std::runtime_error("Cannot open manifest for signing: " + manifestPath.string());
        }

        // CMS_DETACHED is intentionally absent — content is embedded (equivalent to -nodetach)
        CmsPtr cms(CMS_sign(nullptr, nullptr, nullptr, nullptr, CMS_PARTIAL | CMS_BINARY),
                   CMS_ContentInfo_free);
        if (!cms) {
            throw std::runtime_error("CMS_sign init failed: " + opensslError());
        }
        if (!CMS_add1_signer(cms.get(), signerCert.get(), signerKey.get(), EVP_sha256(), 0)) {
            throw std::runtime_error("CMS_add1_signer failed: " + opensslError());
        }
        if (CMS_final(cms.get(), inBio.get(), nullptr, CMS_BINARY) != 1) {
            throw std::runtime_error("CMS_final failed: " + opensslError());
        }

        BioPtr outBio(BIO_new_file(cmsPath.string().c_str(), "wb"), BIO_free);
        if (!outBio) {
            throw std::runtime_error("Cannot write CMS output: " + cmsPath.string());
        }
        if (i2d_CMS_bio(outBio.get(), cms.get()) != 1) {
            throw std::runtime_error("Failed to write CMS DER: " + opensslError());
        }
    }

    std::filesystem::copy_file(unsignedBundlePath, outputBundlePath, std::filesystem::copy_options::overwrite_existing);

    const auto cmsSize = std::filesystem::file_size(cmsPath);
    std::ifstream cmsInput(cmsPath, std::ios::binary);
    if (!cmsInput) {
        throw std::runtime_error("Cannot open generated CMS file: " + cmsPath.string());
    }

    std::ofstream bundleOutput(outputBundlePath, std::ios::binary | std::ios::app);
    if (!bundleOutput) {
        throw std::runtime_error("Cannot append CMS to bundle: " + outputBundlePath.string());
    }

    bundleOutput << cmsInput.rdbuf();

    std::uint64_t sizeValue = cmsSize;
    char trailer[8];
    for (int i = 0; i < 8; ++i) {
        trailer[i] = static_cast<char>((sizeValue >> (i * 8)) & 0xff);
    }
    bundleOutput.write(trailer, sizeof(trailer));
    if (!bundleOutput) {
        throw std::runtime_error("Failed to finalize signed bundle: " + outputBundlePath.string());
    }
}

void BundleCreator::create(const BundleCreateOptions& options) const {
    if (options.outputBundle.empty()) {
        throw std::runtime_error("bundle create requires --output");
    }
    if (!options.manifestPath.empty() && !options.artifacts.empty()) {
        throw std::runtime_error("bundle create accepts either --manifest or --artifact, not both");
    }
    if (options.certPath.empty() != options.keyPath.empty()) {
        throw std::runtime_error("bundle create requires both --cert and --key together");
    }

    const auto uniqueSuffix = std::to_string(std::filesystem::file_time_type::clock::now().time_since_epoch().count());
    const auto workDir = std::filesystem::temp_directory_path() / ("aegis-bundle-" + uniqueSuffix);
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);

    BundleManifest manifest;
    std::filesystem::path sourceDir;
    if (!options.manifestPath.empty()) {
        manifest = loadManifestTemplate(options);
        sourceDir = options.sourceDirectory.empty()
            ? std::filesystem::path(options.manifestPath).parent_path()
            : std::filesystem::path(options.sourceDirectory);
    } else {
        manifest = buildManifestFromArtifacts(options);
        for (std::size_t i = 0; i < options.artifacts.size(); ++i) {
            manifest.images[i].filename = validateRelativeBundlePath(options.artifacts[i].bundleFilename).string();
        }
    }

    if (options.manifestPath.empty()) {
        for (std::size_t i = 0; i < options.artifacts.size(); ++i) {
            const auto& artifact = options.artifacts[i];
            if (!std::filesystem::exists(artifact.path)) {
                throw std::runtime_error("Artifact does not exist: " + artifact.path);
            }

            const auto relativePath = std::filesystem::path(manifest.images[i].filename);
            const auto destPath = workDir / relativePath;
            std::filesystem::create_directories(destPath.parent_path());
            std::filesystem::copy_file(artifact.path, destPath, std::filesystem::copy_options::overwrite_existing);

            manifest.images[i].sha256 = sha256(destPath.string());
            manifest.images[i].size = std::filesystem::file_size(destPath);
        }
    } else {
        stageManifestPayloads(manifest, sourceDir, workDir);
    }

    BundleManifestIO io;
    const auto manifestPath = workDir / "manifest.ini";
    writeFile(manifestPath.string(), io.serialize(manifest));

    const auto outputDir = std::filesystem::path(options.outputBundle).parent_path();
    if (!outputDir.empty()) {
        std::filesystem::create_directories(outputDir);
    }

    const auto unsignedBundlePath = workDir / "bundle-unsigned.aegisb";
    std::string tarCommand = "tar -C " + shellQuote(workDir.string()) + " -czf " + shellQuote(unsignedBundlePath.string()) + " manifest.ini";
    for (const auto& image : manifest.images) {
        tarCommand += " " + shellQuote(validateRelativeBundlePath(image.filename).string());
    }
    runner_.runOrThrow(tarCommand);

    const auto outputBundlePath = std::filesystem::path(options.outputBundle);
    if (!options.certPath.empty()) {
        signBundle(manifestPath, unsignedBundlePath, outputBundlePath, options);
    } else {
        std::filesystem::copy_file(unsignedBundlePath, outputBundlePath, std::filesystem::copy_options::overwrite_existing);
    }
}

}  // namespace aegis
