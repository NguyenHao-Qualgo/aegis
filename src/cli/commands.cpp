#include "aegis/cli/commands.h"

#include "aegis/cli/app_context.h"
#include "aegis/dbus/client.h"

#include "aegis/bundle.h"
#include "aegis/context.h"
#include "aegis/mark.h"
#include "aegis/dbus/service.h"
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

    AegisDbusClient client;

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
        auto last_error = client.get_property_string("LastError");
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
    AegisDbusClient client;

    auto conn = client.connect_system_bus();
    if (!conn) {
        std::cerr << "Error: " << conn.error() << "\n";
        return 1;
    }

    auto compatible = client.get_property_string("Compatible");
    auto variant = client.get_property_string("Variant");
    auto boot_slot = client.get_property_string("BootSlot");
    auto bootloader = client.get_property_string("Bootloader");
    auto primary = client.get_primary();
    auto slots = client.get_slot_status();

    if (!compatible) {
        std::cerr << "Error: " << compatible.error() << "\n";
        return 1;
    }
    if (!variant) {
        std::cerr << "Error: " << variant.error() << "\n";
        return 1;
    }
    if (!boot_slot) {
        std::cerr << "Error: " << boot_slot.error() << "\n";
        return 1;
    }
    if (!bootloader) {
        std::cerr << "Error: " << bootloader.error() << "\n";
        return 1;
    }
    if (!primary) {
        std::cerr << "Error: " << primary.error() << "\n";
        return 1;
    }
    if (!slots) {
        std::cerr << "Error: " << slots.error() << "\n";
        return 1;
    }

    std::cout << "=== System Info ===\n"
              << "Compatible:    " << compatible.value() << "\n"
              << "Variant:       " << variant.value() << "\n"
              << "Bootloader:    " << bootloader.value() << "\n"
              << "Boot slot:     " << boot_slot.value() << "\n"
              << "Primary slot:  " << primary.value() << "\n"
              << "\n=== Slot Status ===\n";

    for (const auto& slot : slots.value()) {
        bool is_booted = false;
        bool is_primary = false;

        auto it_booted = slot.bool_fields.find("booted");
        if (it_booted != slot.bool_fields.end()) {
            is_booted = it_booted->second;
        }

        auto it_primary = slot.bool_fields.find("primary");
        if (it_primary != slot.bool_fields.end()) {
            is_primary = it_primary->second;
        }

        std::cout << "\n  Slot " << slot.name;
        if (is_booted) std::cout << " [BOOTED]";
        if (is_primary) std::cout << " [PRIMARY]";
        std::cout << ":\n";

        auto print_s = [&](const char* key, const char* label) {
            auto it = slot.string_fields.find(key);
            if (it != slot.string_fields.end()) {
                std::cout << "    " << label << ": " << it->second << "\n";
            }
        };

        print_s("device", "device");
        print_s("type", "type");
        print_s("bootname", "bootname");
        print_s("class", "class");
        print_s("state", "state");

        if (opts.detailed) {
            print_s("bundle.compatible", "bundle.compatible");
            print_s("bundle.version", "bundle.version");
            print_s("bundle.description", "bundle.description");
            print_s("bundle.build", "bundle.build");
            print_s("bundle.hash", "bundle.hash");
            print_s("sha256", "sha256");
            print_s("installed.timestamp", "installed.timestamp");
            print_s("activated.timestamp", "activated.timestamp");

            auto print_u32 = [&](const char* key, const char* label) {
                auto it = slot.u32_fields.find(key);
                if (it != slot.u32_fields.end()) {
                    std::cout << "    " << label << ": " << it->second << "\n";
                }
            };

            auto print_u64 = [&](const char* key, const char* label) {
                auto it = slot.u64_fields.find(key);
                if (it != slot.u64_fields.end()) {
                    std::cout << "    " << label << ": " << it->second << "\n";
                }
            };

            auto print_i32 = [&](const char* key, const char* label) {
                auto it = slot.i32_fields.find(key);
                if (it != slot.i32_fields.end()) {
                    std::cout << "    " << label << ": " << it->second << "\n";
                }
            };

            print_i32("index", "index");
            print_u64("size", "size");
            print_u32("installed.count", "installed.count");
            print_u32("activated.count", "activated.count");
        }
    }

    return 0;
}

int MarkCommand::execute(const CliOptions& opts) {
    AegisDbusClient client;

    auto conn = client.connect_system_bus();
    if (!conn) {
        std::cerr << "Error: " << conn.error() << "\n";
        return 1;
    }

    std::string slot_id = opts.positional.empty() ? "" : opts.positional[0];
    std::string state;

    if (opts.command == "mark-good") {
        state = "good";
    } else if (opts.command == "mark-bad") {
        state = "bad";
    } else if (opts.command == "mark-active") {
        if (slot_id.empty()) {
            std::cerr << "Usage: aegis mark-active SLOTNAME\n";
            return 1;
        }
        state = "active";
    } else {
        std::cerr << "Error: unknown mark command\n";
        return 1;
    }

    auto res = client.mark(state, slot_id);
    if (!res) {
        std::cerr << "Error: " << res.error() << "\n";
        return 1;
    }

    std::cout << res.value().message << "\n";
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

int VersionCommand::execute(const CliOptions&) {
    AegisDbusClient client;

    auto conn = client.connect_system_bus();
    if (!conn) {
        std::cerr << "Error: " << conn.error() << "\n";
        return 1;
    }

    auto version = client.get_property_string("ServiceVersion");
    if (!version) {
        std::cerr << "Error: " << version.error() << "\n";
        return 1;
    }

    std::cout << version.value() << "\n";
    return 0;
}

} // namespace aegis
