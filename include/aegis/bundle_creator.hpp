#pragma once

#include <string>
#include <vector>

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
    std::vector<BundleArtifactInput> artifacts;
};

class BundleCreator {
public:
    explicit BundleCreator(CommandRunner runner);

    void create(const BundleCreateOptions& options) const;
    static BundleArtifactInput parseArtifactSpec(const std::string& spec);

private:
    std::string sha256(const std::string& path) const;

    CommandRunner runner_;
};

}  // namespace aegis
