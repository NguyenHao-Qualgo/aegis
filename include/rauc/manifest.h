#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rauc {

/// Bundle format variants
enum class BundleFormat {
    Plain,   ///< Legacy: squashfs + appended CMS
    Verity,  ///< dm-verity protected squashfs
    Crypt,   ///< dm-verity + dm-crypt encrypted
};

const char* to_string(BundleFormat fmt);
BundleFormat bundle_format_from_string(const std::string& s);

/// A single image entry in the manifest
struct ManifestImage {
    std::string slotclass;    ///< e.g. "rootfs"
    std::string filename;
    std::string sha256;
    uint64_t    size = 0;
    std::string variant;      ///< optional variant filtering
    std::vector<std::string> convert;  ///< conversion pipeline
    std::string hooks;        ///< pre/post/install hooks
};

/// Parsed manifest
struct Manifest {
    // [update] section
    std::string compatible;
    std::string version;
    std::string build;
    std::string description;

    // [bundle] section (verity/crypt)
    BundleFormat bundle_format = BundleFormat::Plain;
    std::string  verity_hash;
    std::string  verity_salt;
    uint64_t     bundle_verity_size = 0;
    std::string  crypt_key;   ///< AES-256 key (only in crypt manifests)

    // [hooks] section
    std::string  hook_filename;
    std::vector<std::string> hook_install;

    // [image.*] sections
    std::vector<ManifestImage> images;

    // [handler] section
    std::string handler_name;
    std::string handler_args;
};

/// Parse a manifest from an INI-style file
Manifest parse_manifest(const std::string& path);

/// Write a manifest to a file
void write_manifest(const Manifest& manifest, const std::string& path);

} // namespace rauc
