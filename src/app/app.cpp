#include "aegis/app/app.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "aegis/bootloader/boot_control_factory.hpp"
#include "aegis/app/cli.hpp"
#include "aegis/config/config.hpp"
#include "aegis/service/gcs_stub.hpp"
#include "aegis/core/ota_service.hpp"
#include "aegis/config/state_store.hpp"
#include "aegis/common/util.hpp"
#include "aegis/installer/packer.hpp"

#if defined(AEGIS_ENABLE_DBUS)
#include "aegis/service/dbus_service.hpp"
#endif

namespace aegis {

namespace {
constexpr const char *kDefaultConfigPath = "/etc/skytrack/system.conf";

[[noreturn]] void fail(const std::string &message) {
    LOG_E(message);
    std::exit(EXIT_FAILURE);
}

std::vector<std::string> toArgs(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return args;
}

std::string usage() {
    return
        "Usage:\n"
        "  swupdate install --image <file|-> [--verbose]\n"
        "  swupdate pack --output <file.swu> --sw-description <file> [--sw-description-sig <file>] <payload>...\n"
        "\n"
        "Examples:\n"
        "  swupdate install --image update.swu\n"
        "  swupdate pack --output update.swu --sw-description sw-description --sw-description-sig sw-description.sig rootfs.ext4.enc\n";
}


std::string take_value(int &index, int argc, char **argv, const std::string &flag_name) {
    if (index + 1 >= argc) {
        fail("Missing value for " + flag_name);
    }
    ++index;
    return argv[index];
}

bool is_flag(const char *arg, const char *short_flag, const char *long_flag) {
    return std::strcmp(arg, short_flag) == 0 || std::strcmp(arg, long_flag) == 0;
}

void ensure_readable_file(const std::string &path, const std::string &label) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("cannot open " + label + ": " + path);
    }
}

PackOptions parse_pack(int argc, char **argv, int start_index) {
    PackOptions options;

    for (int i = start_index; i < argc; ++i) {
        if (is_flag(argv[i], "-o", "--output")) {
            options.output_path = take_value(i, argc, argv, "--output");
        } else if (std::strcmp(argv[i], "--sw-description") == 0) {
            options.sw_description = take_value(i, argc, argv, "--sw-description");
        } else if (std::strcmp(argv[i], "--sw-description-sig") == 0) {
            options.sw_description_sig = take_value(i, argc, argv, "--sw-description-sig");
        } else if (is_flag(argv[i], "-h", "--help")) {
            LOG_W(usage());
            std::exit(EXIT_SUCCESS);
        } else {
            options.payloads.emplace_back(argv[i]);
        }
    }

    if (options.output_path.empty()) {
        fail("pack requires --output");
    }
    if (options.sw_description.empty()) {
        fail("pack requires --sw-description");
    }

    ensure_readable_file(options.sw_description, "sw-description");
    if (!options.sw_description_sig.empty()) {
        ensure_readable_file(options.sw_description_sig, "sw-description signature");
    }
    for (const auto &payload : options.payloads) {
        ensure_readable_file(payload, "payload");
    }

    return options;
}

}

int Application::run(int argc, char** argv) {
    const auto args = toArgs(argc, argv);
    if (args.empty()) {
#if defined(AEGIS_ENABLE_DBUS)
        Cli client;
        return client.run(args);
#else
        fail("This build only supports 'bundle create'");
#endif
    }

    if (args[0] == "pack") {
        try {
            return Packer(parse_pack(argc, argv, 2)).pack();
        } catch (const Error& error) {
            fail(error.what());
        }
    }

    if (args[0] == "daemon") {
#if !defined(AEGIS_ENABLE_DBUS)
        throw std::runtime_error("Daemon support is disabled in this build");
#else
        // TODO read conf and set log level accordingly
        AppLog::Init(AppLog::Level::debug, nullptr, "aegis-daemon");
        LOG_I("Starting aegis daemon");

        const auto configPath = getOptionValue(args, "--config").empty() ? std::string(kDefaultConfigPath) : getOptionValue(args, "--config");
        LOG_I("Loading config: " + configPath);
        ConfigLoader loader;
        const auto config = loader.load(configPath);
        std::filesystem::create_directories(config.data_directory);
        CommandRunner runner;
        auto bootControl =  BootControlFactory::create(BootloaderType::UBoot, runner);
        StateStore stateStore(joinPath(config.data_directory, "ota-state.env"));
        auto gcsClient = std::make_shared<GcsStub>();
        OtaService service(config, std::move(bootControl), stateStore, std::move(gcsClient));
        service.resumeAfterBoot();
        DbusService dbus(service);
        dbus.run();
        return 0;
#endif
    }

#if defined(AEGIS_ENABLE_DBUS)
    Cli client;
    return client.run(args);
#else
    throw std::runtime_error("This build only supports 'bundle create'");
#endif
}

}  // namespace aegis
