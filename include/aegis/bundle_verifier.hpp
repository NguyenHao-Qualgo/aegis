#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aegis/bundle_manifest.hpp"
#include "aegis/command_runner.hpp"
#include "aegis/types.hpp"

namespace aegis {

class BundleVerifier {
public:
    struct SignatureInfo {
        std::uint64_t cmsSize{0};
        std::uint64_t cmsOffset{0};
        std::uint64_t payloadSize{0};
    };

    explicit BundleVerifier(CommandRunner runner);

    BundleManifest verifyExtracted(const std::string& bundlePath, const std::string& extractedDir,
                                   const OtaConfig& config) const;
    std::uint64_t payloadSize(const std::string& bundlePath) const;
    std::optional<SignatureInfo> signatureInfo(const std::string& bundlePath) const;

private:
    BundleManifest loadManifestFromCms(const std::string& bundlePath, const OtaConfig& config) const;
    std::string resolvePayloadPath(const std::string& extractedDir, const std::string& filename) const;
    std::string sha256(const std::string& path) const;

    CommandRunner runner_;
};

}  // namespace aegis
