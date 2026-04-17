#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aegis {

struct BundleImage {
    std::string name;
    std::string slotClass;
    std::string type;      // image, file, boot, kernel, none
    std::string imagetype; // raw, archive, file, emptyfs
    std::string filename;
    std::string sha256;
    std::uint64_t size{0};
};

struct BundleManifest {
    std::string version;
    std::string compatible;
    std::string format{"plain"};
    std::vector<BundleImage> images;

    const BundleImage* findImageBySlotClass(const std::string& slotClass) const;
};

enum class BundleManifestValidationMode {
    AllowMissingPayloadMetadata,
    RequirePayloadMetadata,
};

class BundleManifestIO {
public:
    BundleManifest loadFromFile(
        const std::string& path,
        BundleManifestValidationMode validationMode = BundleManifestValidationMode::RequirePayloadMetadata) const;
    std::string serialize(const BundleManifest& manifest) const;
};

}  // namespace aegis
