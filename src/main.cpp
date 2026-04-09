#include "rauc/bundle.h"
#include "rauc/context.h"
#include "rauc/install.h"
#include "rauc/mark.h"
#include "rauc/service.h"
#include "rauc/utils.h"
#include "config.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace rauc;

struct CliOptions {
    std::string command;
    std::string config_path    = "/etc/rauc/system.conf";
    std::string cert_path;
    std::string key_path;
    std::string keyring_path;
    std::string override_boot_slot;
    std::string mount_prefix;
    std::string mksquashfs_args;
    std::string handler_args;
    std::string output_format = "readable";  // readable, shell, json
    std::string bundle_format = "verity";
    bool        detailed       = false;
    bool        ignore_compat  = false;
    bool        no_check_time  = false;
    bool        no_verify      = false;
    std::vector<std::string> positional;
    std::vector<std::string> encryption_recipients;
};

static void print_usage() {
    std::cerr << R"(Usage: rauc [OPTIONS] COMMAND [ARGS]

Commands:
  bundle CONTENTDIR BUNDLEFILE    Create a bundle from content directory
  install BUNDLEFILE              Install a bundle
  info BUNDLEFILE                 Show bundle info
  status                          Show system status
  mark-good [SLOTNAME]            Mark slot as good
  mark-bad [SLOTNAME]             Mark slot as bad
  mark-active SLOTNAME            Set slot as primary boot target
  extract BUNDLEFILE OUTPUTDIR    Extract bundle contents
  resign INBUNDLE OUTBUNDLE       Resign bundle with new keys
  service                         Run as D-Bus service 
  mount BUNDLEFILE                Verify and mount a bundle

Options:
  --conf=PATH                     System config file (default: /etc/rauc/system.conf)
  --cert=PATH                     Signing certificate
  --key=PATH                      Signing key
  --keyring=PATH                  Verification keyring
  --override-boot-slot=BOOTNAME   Override booted slot detection
  --mount=PATH                    Mount prefix
  --bundle-format=FORMAT          Bundle format: plain, verity, crypt (default: verity)
  --mksquashfs-args=ARGS          Extra mksquashfs arguments
  --output-format=FMT             Output format: readable, shell, json
  --detailed                      Show detailed status
  --ignore-compatible             Skip compatible check
  --no-check-time                 Ignore certificate expiry
  --no-verify                     Skip signature verification (info only)
  --recipient=CERTPATH            Encryption recipient (crypt format, repeatable)
  --help                          Show this help
  --version                       Show version
)";
}

static CliOptions parse_args(int argc, char* argv[]) {
    CliOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        }
        if (arg == "--version") {
            std::cout << "rauc-cpp " << RAUC_VERSION << "\n";
            std::exit(0);
        }

        // Long options with '='
        auto try_opt = [&](const std::string& prefix, std::string& target) -> bool {
            if (arg.substr(0, prefix.size()) == prefix) {
                target = arg.substr(prefix.size());
                return true;
            }
            return false;
        };

        if (try_opt("--conf=", opts.config_path)) continue;
        if (try_opt("--cert=", opts.cert_path)) continue;
        if (try_opt("--key=", opts.key_path)) continue;
        if (try_opt("--keyring=", opts.keyring_path)) continue;
        if (try_opt("--override-boot-slot=", opts.override_boot_slot)) continue;
        if (try_opt("--mount=", opts.mount_prefix)) continue;
        if (try_opt("--bundle-format=", opts.bundle_format)) continue;
        if (try_opt("--mksquashfs-args=", opts.mksquashfs_args)) continue;
        if (try_opt("--output-format=", opts.output_format)) continue;
        if (try_opt("--handler-args=", opts.handler_args)) continue;

        if (arg.substr(0, 12) == "--recipient=") {
            opts.encryption_recipients.push_back(arg.substr(12));
            continue;
        }

        if (arg == "--detailed")          { opts.detailed = true; continue; }
        if (arg == "--ignore-compatible") { opts.ignore_compat = true; continue; }
        if (arg == "--no-check-time")     { opts.no_check_time = true; continue; }
        if (arg == "--no-verify")         { opts.no_verify = true; continue; }

        // Positional arguments
        if (opts.command.empty()) {
            opts.command = arg;
        } else {
            opts.positional.push_back(arg);
        }
    }
    return opts;
}

static int cmd_bundle(const CliOptions& opts) {
    if (opts.positional.size() < 2) {
        std::cerr << "Usage: rauc bundle CONTENTDIR BUNDLEFILE\n";
        return 1;
    }

    BundleCreateParams params;
    params.content_dir  = opts.positional[0];
    params.output_path  = opts.positional[1];
    params.cert_path    = opts.cert_path;
    params.key_path     = opts.key_path;
    params.format       = bundle_format_from_string(opts.bundle_format);
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

static int cmd_install(const CliOptions& opts) {
    if (opts.positional.empty()) {
        std::cerr << "Usage: rauc install BUNDLEFILE\n";
        return 1;
    }

    Context::instance().init(
        opts.config_path, opts.cert_path, opts.key_path,
        opts.keyring_path, opts.override_boot_slot, opts.mount_prefix);

    InstallArgs args;
    args.name = opts.positional[0];
    args.ignore_compatible = opts.ignore_compat;
    args.progress = [](int pct, const std::string& msg) {
        std::cout << "[" << pct << "%] " << msg << "\n";
    };
    args.status_notify = [](const std::string& msg) {
        std::cout << ">> " << msg << "\n";
    };

    auto res = install_bundle(opts.positional[0], args);
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    std::cout << "Installation successful.\n";
    return 0;
}

static int cmd_info(const CliOptions& opts) {
    if (opts.positional.empty()) {
        std::cerr << "Usage: rauc info BUNDLEFILE\n";
        return 1;
    }

    SigningParams params;
    params.keyring_path   = opts.keyring_path;
    params.no_check_time  = opts.no_check_time;

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

static int cmd_status(const CliOptions& opts) {
    Context::instance().init(
        opts.config_path, {}, {}, opts.keyring_path,
        opts.override_boot_slot, opts.mount_prefix);

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

static int cmd_mark(const CliOptions& opts) {
    Context::instance().init(
        opts.config_path, {}, {}, opts.keyring_path,
        opts.override_boot_slot, opts.mount_prefix);

    std::string slot_id = opts.positional.empty() ? "" : opts.positional[0];

    Result<void> res = Result<void>::err("Unknown mark command");
    if (opts.command == "mark-good") {
        res = mark_good(slot_id);
    } else if (opts.command == "mark-bad") {
        res = mark_bad(slot_id);
    } else if (opts.command == "mark-active") {
        if (slot_id.empty()) {
            std::cerr << "Usage: rauc mark-active SLOTNAME\n";
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

static int cmd_extract(const CliOptions& opts) {
    if (opts.positional.size() < 2) {
        std::cerr << "Usage: rauc extract BUNDLEFILE OUTPUTDIR\n";
        return 1;
    }

    SigningParams params;
    params.keyring_path  = opts.keyring_path;
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

static int cmd_resign(const CliOptions& opts) {
    if (opts.positional.size() < 2) {
        std::cerr << "Usage: rauc resign INBUNDLE OUTBUNDLE\n";
        return 1;
    }

    SigningParams old_params;
    old_params.keyring_path  = opts.keyring_path;
    old_params.no_check_time = opts.no_check_time;

    SigningParams new_params;
    new_params.cert_path = opts.cert_path;
    new_params.key_path  = opts.key_path;

    if (new_params.cert_path.empty() || new_params.key_path.empty()) {
        std::cerr << "Error: --cert and --key required for resign\n";
        return 1;
    }

    auto res = bundle_resign(opts.positional[0], opts.positional[1],
                              old_params, new_params);
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }
    std::cout << "Bundle resigned: " << opts.positional[1] << "\n";
    return 0;
}

static int cmd_service(const CliOptions& opts) {
    Context::instance().init(
        opts.config_path, opts.cert_path, opts.key_path,
        opts.keyring_path, opts.override_boot_slot, opts.mount_prefix);

    auto res = service_run();
    if (!res) {
        std::cerr << "Service error: " << res.error() << "\n";
        return 1;
    }
    return 0;
}

static int cmd_mount(const CliOptions& opts) {
    if (opts.positional.empty()) {
        std::cerr << "Usage: rauc mount BUNDLEFILE\n";
        return 1;
    }

    Context::instance().init(
        opts.config_path, {}, {}, opts.keyring_path,
        opts.override_boot_slot, opts.mount_prefix);

    SigningParams params;
    params.keyring_path  = opts.keyring_path;
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    auto opts = parse_args(argc, argv);

    try {
        if (opts.command == "bundle")      return cmd_bundle(opts);
        if (opts.command == "install")     return cmd_install(opts);
        if (opts.command == "info")        return cmd_info(opts);
        if (opts.command == "status")      return cmd_status(opts);
        if (opts.command == "mark-good" ||
            opts.command == "mark-bad"  ||
            opts.command == "mark-active") return cmd_mark(opts);
        if (opts.command == "extract")     return cmd_extract(opts);
        if (opts.command == "resign")      return cmd_resign(opts);
        if (opts.command == "service")     return cmd_service(opts);
        if (opts.command == "mount")       return cmd_mount(opts);

        std::cerr << "Unknown command: " << opts.command << "\n";
        print_usage();
        return 1;

    } catch (const RaucError& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }
}
