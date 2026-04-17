#pragma once

#include <string>

#include "aegis/bundle_manifest.hpp"
#include "aegis/command_runner.hpp"
#include "aegis/types.hpp"

namespace aegis {

class BundleVerifier {
public:
    explicit BundleVerifier(CommandRunner runner);

    BundleManifest verifyExtracted(const std::string& extractedDir, const OtaConfig& config) const;

private:
    std::string sha256(const std::string& path) const;

    CommandRunner runner_;
};

}  // namespace aegis
