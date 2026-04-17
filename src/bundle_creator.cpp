#include "aegis/bundle_creator.hpp"

#include <filesystem>
#include <stdexcept>

#include "aegis/bundle_manifest.hpp"
#include "aegis/util.hpp"

namespace aegis {

BundleCreator::BundleCreator(CommandRunner runner) : runner_(std::move(runner)) {}

std::string BundleCreator::sha256(const std::string& path) const {
    const auto output = trim(runner_.runOrThrow("sha256sum " + shellQuote(path) + " | awk '{print $1}'"));
    if (output.empty()) {
        throw std::runtime_error("Failed to calculate sha256");
    }
    return output;
}

BundleArtifactInput BundleCreator::parseArtifactSpec(const std::string& spec) {
    const auto first = spec.find(':');
    const auto second = first == std::string::npos ? std::string::npos : spec.find(':', first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        throw std::runtime_error("Invalid --artifact spec: expected <slot-class>:<type>:<path>");
    }

    BundleArtifactInput input;
    input.slotClass = trim(spec.substr(0, first));
    input.imagetype = trim(spec.substr(first + 1, second - first - 1));
    input.path = trim(spec.substr(second + 1));
    if (input.slotClass.empty() || input.imagetype.empty() || input.path.empty()) {
        throw std::runtime_error("Invalid --artifact spec: empty field");
    }
    input.bundleFilename = std::filesystem::path(input.path).filename().string();
    if (input.bundleFilename.empty()) {
        throw std::runtime_error("Artifact path has no filename: " + input.path);
    }
    return input;
}

void BundleCreator::create(const BundleCreateOptions& options) const {
    if (options.compatible.empty() || options.version.empty() || options.outputBundle.empty()) {
        throw std::runtime_error("bundle create requires compatible, version and output");
    }
    if (options.artifacts.empty()) {
        throw std::runtime_error("bundle create requires at least one --artifact");
    }

    const auto workDir = std::filesystem::temp_directory_path() / ("aegis-bundle-" + options.version);
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);

    BundleManifest manifest;
    manifest.version = options.version;
    manifest.compatible = options.compatible;
    manifest.format = options.format;

    for (const auto& artifact : options.artifacts) {
        if (!std::filesystem::exists(artifact.path)) {
            throw std::runtime_error("Artifact does not exist: " + artifact.path);
        }
        const auto dest = workDir / artifact.bundleFilename;
        std::filesystem::copy_file(artifact.path, dest, std::filesystem::copy_options::overwrite_existing);

        BundleImage image;
        image.name = artifact.slotClass;
        image.slotClass = artifact.slotClass;
        image.type = artifact.sourceType;
        image.imagetype = artifact.imagetype;
        image.filename = artifact.bundleFilename;
        image.sha256 = sha256(dest.string());
        image.size = std::filesystem::file_size(dest);
        manifest.images.push_back(std::move(image));
    }

    BundleManifestIO io;
    writeFile((workDir / "manifest.ini").string(), io.serialize(manifest));

    const auto outputDir = std::filesystem::path(options.outputBundle).parent_path();
    if (!outputDir.empty()) {
        std::filesystem::create_directories(outputDir);
    }

    std::string tarCommand = "tar -C " + shellQuote(workDir.string()) + " -czf " + shellQuote(options.outputBundle) + " manifest.ini";
    for (const auto& artifact : options.artifacts) {
        tarCommand += " " + shellQuote(artifact.bundleFilename);
    }
    runner_.runOrThrow(tarCommand);
}

}  // namespace aegis
