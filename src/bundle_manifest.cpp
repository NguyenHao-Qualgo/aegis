#include "aegis/bundle_manifest.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "aegis/util.hpp"

namespace aegis {

const BundleImage* BundleManifest::findImageBySlotClass(const std::string& slotClass) const {
    const auto it = std::find_if(images.begin(), images.end(), [&](const BundleImage& image) {
        return image.slotClass == slotClass;
    });
    return it == images.end() ? nullptr : &*it;
}

namespace {
void validateManifest(const BundleManifest& manifest, BundleManifestValidationMode validationMode) {
    if (manifest.version.empty() || manifest.compatible.empty() || manifest.images.empty()) {
        throw std::runtime_error("Manifest is incomplete");
    }

    for (const auto& image : manifest.images) {
        if (image.slotClass.empty() || image.filename.empty()) {
            throw std::runtime_error("Manifest image entry is incomplete: " + image.name);
        }
        if (validationMode == BundleManifestValidationMode::RequirePayloadMetadata &&
            (image.sha256.empty() || image.size == 0)) {
            throw std::runtime_error("Manifest image entry is incomplete: " + image.name);
        }
    }
}
}

BundleManifest BundleManifestIO::loadFromFile(const std::string& path, BundleManifestValidationMode validationMode) const {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Cannot open manifest: " + path);
    }

    BundleManifest manifest;
    BundleImage* currentImage = nullptr;
    std::string line;
    std::string section;
    while (std::getline(ifs, line)) {
        line = trim(line);
        if (line.empty() || startsWith(line, "#") || startsWith(line, ";")) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            currentImage = nullptr;
            if (startsWith(section, "image.")) {
                manifest.images.push_back(BundleImage{});
                manifest.images.back().name = section.substr(6);
                currentImage = &manifest.images.back();
            }
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        const auto key = trim(line.substr(0, pos));
        const auto value = trim(line.substr(pos + 1));
        if (section == "bundle" || section == "update") {
            if (key == "version") manifest.version = value;
            else if (key == "compatible") manifest.compatible = value;
            else if (key == "format") manifest.format = value;
        } else if (currentImage != nullptr) {
            if (key == "slot-class") currentImage->slotClass = value;
            else if (key == "type") currentImage->imagetype = value;
            else if (key == "source-type") currentImage->sourceType = value;
            else if (key == "filename") currentImage->filename = value;
            else if (key == "sha256") currentImage->sha256 = value;
            else if (key == "size") currentImage->size = std::stoull(value);
        }
    }

    validateManifest(manifest, validationMode);
    return manifest;
}

std::string BundleManifestIO::serialize(const BundleManifest& manifest) const {
    std::ostringstream os;
    os << "[update]\n";
    os << "compatible=" << manifest.compatible << "\n";
    os << "version=" << manifest.version << "\n\n";
    os << "[bundle]\n";
    os << "format=" << manifest.format << "\n\n";
    for (const auto& image : manifest.images) {
        os << "[image." << image.name << "]\n";
        os << "slot-class=" << image.slotClass << "\n";
        if (!image.sourceType.empty()) {
            os << "source-type=" << image.sourceType << "\n";
        }
        if (!image.imagetype.empty()) {
            os << "type=" << image.imagetype << "\n";
        }
        os << "filename=" << image.filename << "\n";
        os << "sha256=" << image.sha256 << "\n";
        os << "size=" << image.size << "\n\n";
    }
    return os.str();
}

}  // namespace aegis
