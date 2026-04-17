#include "aegis/bundle_verifier.hpp"

#include <filesystem>
#include <stdexcept>

#include "aegis/util.hpp"

namespace aegis {

BundleVerifier::BundleVerifier(CommandRunner runner) : runner_(std::move(runner)) {}

std::string BundleVerifier::sha256(const std::string& path) const {
    return trim(runner_.runOrThrow("sha256sum " + shellQuote(path) + " | awk '{print $1}'"));
}

BundleManifest BundleVerifier::verifyExtracted(const std::string& extractedDir, const OtaConfig& config) const {
    BundleManifestIO io;
    const auto manifest = io.loadFromFile(joinPath(extractedDir, "manifest.ini"));
    if (manifest.compatible != config.compatible) {
        throw std::runtime_error("Bundle compatible mismatch");
    }

    for (const auto& image : manifest.images) {
        const auto payloadPath = joinPath(extractedDir, image.filename);
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
