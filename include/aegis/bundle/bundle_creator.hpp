#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "aegis/bundle/bundle_manifest.hpp"
#include "aegis/command_runner.hpp"

namespace aegis {

struct BundleArtifactInput {
    std::string slotClass;
    std::string imagetype;  // raw, archive, file
    std::string path;
    std::string sourceType{"file"};
    std::string bundleFilename;
};

struct BundleCreateOptions {
    std::string compatible;
    std::string version;
    std::string format{"plain"};
    std::string outputBundle;
    std::string manifestPath;
    std::string sourceDirectory;
    std::string certPath;
    std::string keyPath;
    std::vector<BundleArtifactInput> artifacts;
};

class BundleCreator {
public:
    explicit BundleCreator(CommandRunner runner);

    void create(const BundleCreateOptions& options) const;
    static BundleArtifactInput parseArtifactSpec(const std::string& spec);

private:
    BundleManifest loadManifestTemplate(const BundleCreateOptions& options) const;
    BundleManifest buildManifestFromArtifacts(const BundleCreateOptions& options) const;
    void stageManifestPayloads(BundleManifest& manifest, const std::filesystem::path& sourceDir,
                               const std::filesystem::path& workDir) const;
    void signBundle(const std::filesystem::path& manifestPath, const std::filesystem::path& unsignedBundlePath,
                    const std::filesystem::path& outputBundlePath, const BundleCreateOptions& options) const;
    static std::filesystem::path validateRelativeBundlePath(const std::string& filename);
    std::string sha256(const std::string& path) const;

    CommandRunner runner_;
};

}  // namespace aegis
