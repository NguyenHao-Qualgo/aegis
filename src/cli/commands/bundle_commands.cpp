#include "aegis/cli/commands.h"

#include "aegis/bundle.h"
#include "aegis/cli/app_context.h"

#include <iostream>

namespace aegis {
namespace {

bool require_positional(const CliOptions& opts, std::size_t count, const char* usage) {
    if (opts.positional.size() < count) {
        std::cerr << "Usage: " << usage << "\n";
        return false;
    }
    return true;
}

} // namespace

int BundleCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 2, "aegis bundle CONTENTDIR BUNDLEFILE")) {
        return 1;
    }

    BundleCreateParams params;
    params.content_dir = opts.positional[0];
    params.output_path = opts.positional[1];
    params.cert_path = opts.cert_path;
    params.key_path = opts.key_path;
    params.format = bundle_format_from_string(opts.bundle_format);
    params.mksquashfs_args = opts.mksquashfs_args;
    params.encryption_recipients = opts.encryption_recipients;

    if (params.cert_path.empty() || params.key_path.empty()) {
        std::cerr << "Error: --cert and --key are required for bundle creation\n";
        return 1;
    }

    auto result = bundle_create(params);
    if (!result) {
        std::cerr << "Error: " << result.error() << "\n";
        return 1;
    }

    return 0;
}

int InfoCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 1, "aegis info BUNDLEFILE")) {
        return 1;
    }

    SigningParams params;
    params.keyring_path = opts.keyring_path;
    params.no_check_time = opts.no_check_time;

    auto result = bundle_info(opts.positional[0], params, opts.no_verify);
    if (!result) {
        std::cerr << "Error: " << result.error() << "\n";
        return 1;
    }

    const auto& bundle = result.value();
    std::cout << "Compatible:  " << bundle.manifest.compatible << "\n"
              << "Version:     " << bundle.manifest.version << "\n"
              << "Build:       " << bundle.manifest.build << "\n"
              << "Description: " << bundle.manifest.description << "\n"
              << "Format:      " << to_string(bundle.format) << "\n"
              << "Bundle size: " << bundle.size << " bytes\n"
              << "Verified:    " << (bundle.verified ? "yes" : "no") << "\n"
              << "\nImages:\n";

    for (const auto& image : bundle.manifest.images) {
        std::cout << "  [" << image.slotclass << "]\n"
                  << "    filename: " << image.filename << "\n"
                  << "    sha256:   " << image.sha256 << "\n"
                  << "    size:     " << image.size << "\n";
    }

    return 0;
}

int ExtractCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 2, "aegis extract BUNDLEFILE OUTPUTDIR")) {
        return 1;
    }

    SigningParams params;
    params.keyring_path = opts.keyring_path;
    params.no_check_time = opts.no_check_time;

    auto bundle_result = bundle_open(opts.positional[0], params);
    if (!bundle_result) {
        std::cerr << "Error: " << bundle_result.error() << "\n";
        return 1;
    }

    auto& bundle = bundle_result.value();
    auto mount_result = bundle_mount(bundle);
    if (!mount_result) {
        std::cerr << "Error: " << mount_result.error() << "\n";
        return 1;
    }

    auto extract_result = bundle_extract(bundle, opts.positional[1]);
    bundle_unmount(bundle);

    if (!extract_result) {
        std::cerr << "Error: " << extract_result.error() << "\n";
        return 1;
    }

    std::cout << "Extracted to " << opts.positional[1] << "\n";
    return 0;
}

int ResignCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 2, "aegis resign INBUNDLE OUTBUNDLE")) {
        return 1;
    }

    SigningParams old_params;
    old_params.keyring_path = opts.keyring_path;
    old_params.no_check_time = opts.no_check_time;

    SigningParams new_params;
    new_params.cert_path = opts.cert_path;
    new_params.key_path = opts.key_path;

    if (new_params.cert_path.empty() || new_params.key_path.empty()) {
        std::cerr << "Error: --cert and --key required for resign\n";
        return 1;
    }

    auto result = bundle_resign(opts.positional[0], opts.positional[1], old_params, new_params);
    if (!result) {
        std::cerr << "Error: " << result.error() << "\n";
        return 1;
    }

    std::cout << "Bundle resigned: " << opts.positional[1] << "\n";
    return 0;
}

int MountCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 1, "aegis mount BUNDLEFILE")) {
        return 1;
    }

    AppContext::init_runtime(opts);

    SigningParams params;
    params.keyring_path = opts.keyring_path;
    params.no_check_time = opts.no_check_time;

    auto bundle_result = bundle_open(opts.positional[0], params);
    if (!bundle_result) {
        std::cerr << "Error: " << bundle_result.error() << "\n";
        return 1;
    }

    auto& bundle = bundle_result.value();
    auto mount_result = bundle_mount(bundle);
    if (!mount_result) {
        std::cerr << "Error: " << mount_result.error() << "\n";
        return 1;
    }

    std::cout << bundle.mount_point << "\n";
    return 0;
}

} // namespace aegis
