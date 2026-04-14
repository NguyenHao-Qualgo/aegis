#include "aegis/commands.h"

#include "aegis/app_context.h"
#include "aegis/installer_dbus_client.h"

#include "aegis/bundle.h"
#include "aegis/context.h"
#include "aegis/mark.h"
#include "aegis/service.h"
#include "aegis/utils.h"

#include <iostream>
#include <string>

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

    auto res = bundle_create(params);
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    return 0;
}

int InstallCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 1, "aegis install BUNDLEFILE")) {
        return 1;
    }

    InstallerDbusClient client;

    auto res = client.connect_system_bus();
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    res = client.subscribe_completed();
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    res = client.install_bundle(opts.positional[0], opts.ignore_compat);
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    std::cout << "Install request sent to Aegis service.\n";

    auto completion = client.wait_for_completed();
    if (!completion) {
        std::cerr << "Error: " << completion.error() << "\n";
        return 1;
    }

    if (completion.value() != 0) {
        auto last_error = client.get_last_error();
        if (last_error && !last_error.value().empty()) {
            std::cerr << "Error: " << last_error.value() << "\n";
        } else {
            std::cerr << "Error: installation failed.\n";
        }
        return 1;
    }

    std::cout << "Installation successful.\n";
    return 0;
}

int InfoCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 1, "aegis info BUNDLEFILE")) {
        return 1;
    }

    SigningParams params;
    params.keyring_path = opts.keyring_path;
    params.no_check_time = opts.no_check_time;

    auto res = bundle_info(opts.positional[0], params, opts.no_verify);
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    auto& bundle = res.value();
    std::cout << "Compatible:  " << bundle.manifest.compatible << "\n"
              << "Version:     " << bundle.manifest.version << "\n"
              << "Build:       " << bundle.manifest.build << "\n"
              << "Description: " << bundle.manifest.description << "\n"
              << "Format:      " << to_string(bundle.format) << "\n"
              << "Bundle size: " << bundle.size << " bytes\n"
              << "Verified:    " << (bundle.verified ? "yes" : "no") << "\n"
              << "\nImages:\n";

    for (auto& img : bundle.manifest.images) {
        std::cout << "  [" << img.slotclass << "]\n"
                  << "    filename: " << img.filename << "\n"
                  << "    sha256:   " << img.sha256 << "\n"
                  << "    size:     " << img.size << "\n";
    }

    return 0;
}

int StatusCommand::execute(const CliOptions& opts) {
    AppContext::init_runtime(opts);

    auto& ctx = Context::instance();
    auto& config = ctx.config();

    std::cout << "=== System Info ===\n"
              << "Compatible:    " << config.compatible << "\n"
              << "Bootloader:    " << to_string(config.bootloader) << "\n"
              << "Boot slot:     " << ctx.boot_slot() << "\n"
              << "\n=== Slot Status ===\n";

    for (auto& [name, slot] : config.slots) {
        std::cout << "\n  Slot " << name
                  << (slot.is_booted ? " [BOOTED]" : "") << ":\n"
                  << "    device:     " << slot.device << "\n"
                  << "    type:       " << to_string(slot.type) << "\n"
                  << "    bootname:   " << slot.bootname << "\n";

        if (opts.detailed) {
            auto& s = slot.status;
            std::cout << "    status:     " << s.status << "\n"
                      << "    version:    " << s.bundle_version << "\n"
                      << "    compatible: " << s.bundle_compatible << "\n"
                      << "    sha256:     " << s.checksum_sha256 << "\n"
                      << "    installed:  " << s.installed_timestamp
                      << " (count=" << s.installed_count << ")\n"
                      << "    activated:  " << s.activated_timestamp
                      << " (count=" << s.activated_count << ")\n";
        }
    }

    return 0;
}

int MarkCommand::execute(const CliOptions& opts) {
    AppContext::init_runtime(opts);

    std::string slot_id = opts.positional.empty() ? "" : opts.positional[0];

    Result<void> res = Result<void>::err("Unknown mark command");
    if (opts.command == "mark-good") {
        res = mark_good(slot_id);
    } else if (opts.command == "mark-bad") {
        res = mark_bad(slot_id);
    } else if (opts.command == "mark-active") {
        if (slot_id.empty()) {
            std::cerr << "Usage: aegis mark-active SLOTNAME\n";
            return 1;
        }
        res = mark_active(slot_id);
    }

    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
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

    auto bundle_res = bundle_open(opts.positional[0], params);
    if (!bundle_res) {
        std::cerr << "Error: " << bundle_res.error() << "\n";
        return 1;
    }

    auto& bundle = bundle_res.value();
    auto mount_res = bundle_mount(bundle);
    if (!mount_res) {
        std::cerr << "Error: " << mount_res.error() << "\n";
        return 1;
    }

    auto extract_res = bundle_extract(bundle, opts.positional[1]);
    bundle_unmount(bundle);

    if (!extract_res) {
        std::cerr << "Error: " << extract_res.error() << "\n";
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

    auto res = bundle_resign(opts.positional[0], opts.positional[1], old_params, new_params);
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    std::cout << "Bundle resigned: " << opts.positional[1] << "\n";
    return 0;
}

int ServiceCommand::execute(const CliOptions& opts) {
    AppContext::init_service(opts);

    auto res = service_run();
    if (!res) {
        std::cerr << "Service error: " << res.error() << "\n";
        return 1;
    }

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

    auto bundle_res = bundle_open(opts.positional[0], params);
    if (!bundle_res) {
        std::cerr << "Error: " << bundle_res.error() << "\n";
        return 1;
    }

    auto& bundle = bundle_res.value();
    auto mount_res = bundle_mount(bundle);
    if (!mount_res) {
        std::cerr << "Error: " << mount_res.error() << "\n";
        return 1;
    }

    std::cout << bundle.mount_point << "\n";
    return 0;
}

} // namespace aegis