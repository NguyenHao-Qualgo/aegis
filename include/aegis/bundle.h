#pragma once

#include "aegis/dm.h"
#include "aegis/error.h"
#include "aegis/manifest.h"
#include "aegis/signature.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aegis {

struct Bundle {
    std::string path;
    uint64_t size = 0;

    Manifest manifest;
    BundleFormat format = BundleFormat::Plain;
    bool verified = false;

    bool mounted = false;
    std::string mount_point;

    // Backing block device prepared elsewhere for verity/crypt
    std::string bundle_device;

    LoopDevice loop;
    DmTarget dm_verity;
    DmTarget dm_crypt;
};

struct BundleCreateParams {
    std::string content_dir;
    std::string output_path;

    BundleFormat format = BundleFormat::Verity;

    std::string cert_path;
    std::string key_path;

    std::vector<std::string> encryption_recipients;
    std::string mksquashfs_args;
};

Result<void> bundle_create(const BundleCreateParams& params);

Result<Bundle> bundle_open(const std::string& path, const SigningParams& verify_params);

Result<void> bundle_mount(Bundle& bundle);

Result<void> bundle_unmount(Bundle& bundle);

Result<void> bundle_extract(const Bundle& bundle, const std::string& dest_dir);

Result<void> bundle_resign(const std::string& input_path, const std::string& output_path,
                           const SigningParams& old_params, const SigningParams& new_params);

Result<Bundle> bundle_info(const std::string& path, const SigningParams& params,
                           bool no_verify = false);

} // namespace aegis