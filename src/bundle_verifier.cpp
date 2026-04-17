#include "aegis/bundle_verifier.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "aegis/util.hpp"

namespace aegis {

BundleVerifier::BundleVerifier(CommandRunner runner) : runner_(std::move(runner)) {}

std::string BundleVerifier::sha256(const std::string& path) const {
    return trim(runner_.runOrThrow("sha256sum " + shellQuote(path) + " | awk '{print $1}'"));
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

BundleManifest BundleVerifier::loadManifestFromCms(const std::string& bundlePath, const OtaConfig& config) const {
    if (config.keyringPath.empty()) {
        throw std::runtime_error("Signed bundle verification requires keyring.path in config");
    }

    const auto info = signatureInfo(bundlePath);
    if (!info) {
        throw std::runtime_error("Bundle is not CMS-signed");
    }

    const auto workDir = std::filesystem::temp_directory_path() /
                         ("aegis-verify-" + std::to_string(std::filesystem::file_time_type::clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(workDir);

    const auto cmsPath = workDir / "manifest.cms";
    const auto manifestPath = workDir / "manifest.ini";

    {
        std::ifstream input(bundlePath, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot open bundle: " + bundlePath);
        }
        input.seekg(static_cast<std::streamoff>(info->cmsOffset), std::ios::beg);

        std::ofstream output(cmsPath, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Cannot write temporary CMS file: " + cmsPath.string());
        }

        constexpr std::size_t bufferSize = 8192;
        char buffer[bufferSize];
        std::uint64_t remaining = info->cmsSize;
        while (remaining > 0) {
            const auto chunk = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, bufferSize));
            input.read(buffer, chunk);
            if (input.gcount() != chunk) {
                throw std::runtime_error("Failed to extract CMS from bundle");
            }
            output.write(buffer, chunk);
            remaining -= static_cast<std::uint64_t>(chunk);
        }
    }

    runner_.runOrThrow(
        "openssl cms -verify -binary -inform DER -in " + shellQuote(cmsPath.string()) +
        " -CAfile " + shellQuote(config.keyringPath) +
        " -out " + shellQuote(manifestPath.string()));

    BundleManifestIO io;
    const auto manifest = io.loadFromFile(manifestPath.string());
    std::filesystem::remove_all(workDir);
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

BundleManifest BundleVerifier::verifyExtracted(const std::string& bundlePath, const std::string& extractedDir,
                                               const OtaConfig& config) const {
    BundleManifest manifest;
    if (signatureInfo(bundlePath).has_value()) {
        manifest = loadManifestFromCms(bundlePath, config);
    } else {
        if (!config.keyringPath.empty()) {
            throw std::runtime_error("Unsigned bundle rejected because keyring.path is configured");
        }
        BundleManifestIO io;
        manifest = io.loadFromFile(joinPath(extractedDir, "manifest.ini"));
    }

    if (manifest.compatible != config.compatible) {
        throw std::runtime_error("Bundle compatible mismatch");
    }

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
    return manifest;
}

}  // namespace aegis
