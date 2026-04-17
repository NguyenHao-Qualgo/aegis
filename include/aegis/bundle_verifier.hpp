#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aegis/bundle_manifest.hpp"
#include "aegis/types.hpp"

namespace aegis {

class BundleVerifier {
public:
    struct SignatureInfo {
        std::uint64_t cmsSize{0};
        std::uint64_t cmsOffset{0};
        std::uint64_t payloadSize{0};
    };

    BundleVerifier() = default;

    // Verify CMS signature and return trusted manifest. Returns nullopt for unsigned bundles
    // when no keyring is configured (dev mode). Throws if unsigned but keyring is required.
    std::optional<BundleManifest> verifyBundle(const std::string& bundlePath, const OtaConfig& config) const;

    // Load manifest from extracted archive and check compatible field (unsigned bundles only).
    BundleManifest loadManifest(const std::string& extractedDir, const OtaConfig& config) const;

    // Verify extracted payload files match the manifest checksums.
    void verifyPayloads(const BundleManifest& manifest, const std::string& extractedDir) const;

    std::uint64_t payloadSize(const std::string& bundlePath) const;
    std::optional<SignatureInfo> signatureInfo(const std::string& bundlePath) const;

private:
    std::string resolvePayloadPath(const std::string& extractedDir, const std::string& filename) const;
    std::string sha256(const std::string& path) const;
};

}  // namespace aegis
