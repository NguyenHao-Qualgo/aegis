#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aegis/bundle_manifest.hpp"
#include "aegis/types.hpp"

namespace aegis {

class IBundleVerifier {
public:
    virtual ~IBundleVerifier() = default;

    virtual std::optional<BundleManifest> verifyBundle(const std::string& bundlePath, const OtaConfig& config) const = 0;
    virtual BundleManifest loadManifest(const std::string& extractedDir, const OtaConfig& config) const = 0;
    virtual void verifyPayloads(const BundleManifest& manifest, const std::string& extractedDir) const = 0;
    virtual std::uint64_t payloadSize(const std::string& bundlePath) const = 0;
};

class BundleVerifier : public IBundleVerifier {
public:
    struct SignatureInfo {
        std::uint64_t cmsSize{0};
        std::uint64_t cmsOffset{0};
        std::uint64_t payloadSize{0};
    };

    BundleVerifier() = default;

    std::optional<BundleManifest> verifyBundle(const std::string& bundlePath, const OtaConfig& config) const override;
    BundleManifest loadManifest(const std::string& extractedDir, const OtaConfig& config) const override;
    void verifyPayloads(const BundleManifest& manifest, const std::string& extractedDir) const override;
    std::uint64_t payloadSize(const std::string& bundlePath) const override;

    std::optional<SignatureInfo> signatureInfo(const std::string& bundlePath) const;

private:
    std::string resolvePayloadPath(const std::string& extractedDir, const std::string& filename) const;
    std::string sha256(const std::string& path) const;
};

}  // namespace aegis
