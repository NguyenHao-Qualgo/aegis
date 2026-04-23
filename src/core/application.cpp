#include "aegis/core/application.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <csignal>
#include <string>
#include <string_view>

#include "aegis/bootloader/boot_control_factory.hpp"
#include "aegis/core/cli.hpp"
#include "aegis/common/config.hpp"
#include "aegis/common/signal_set.hpp"
#include "aegis/stub/gcs_stub.hpp"
#include "aegis/core/ota_service.hpp"
#include "aegis/common/state_store.hpp"
#include "aegis/common/util.hpp"
#include "aegis/installer/packer.hpp"

#if defined(AEGIS_ENABLE_DBUS)
#include "aegis/core/dbus_service.hpp"
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

std::string daemon_config_path(const std::vector<std::string>& args) {
    const std::string config_path = getOptionValue(args, "--config");
    return config_path.empty() ? std::string(kDefaultConfigPath) : config_path;
}

OtaService make_ota_service(const std::vector<std::string>& args) {
    const std::string config_path = daemon_config_path(args);
    LOG_I("Loading config: " + config_path);

    ConfigLoader loader;
    const OtaConfig config = loader.load(config_path);
    std::filesystem::create_directories(config.data_directory);

    CommandRunner runner;
    auto boot_control = BootControlFactory::create(config.bootloader_type, runner);
    StateStore state_store(joinPath(config.data_directory, "ota-state.env"));
    auto gcs_client = std::make_shared<GcsStub>();
    return OtaService(config, std::move(boot_control), std::move(state_store), std::move(gcs_client));
}

#if defined(AEGIS_ENABLE_DBUS)
SignalSet make_daemon_signal_set() {
    SignalSet signal_set{SIGINT, SIGTERM};
    signal_set.block();
    return signal_set;
}

void start_daemon_signal_forwarder(DbusService& dbus, SignalSet signal_set) {
    std::thread([&dbus, signal_set]() mutable {
        const int signum = signal_set.wait();
        LOG_I("Received stop signal " + std::to_string(signum) + ", stopping daemon");
        dbus.stop();
    }).detach();
}
#endif

}  // namespace

int Application::runPack(int argc, char** argv) const {
    try {
        return Packer(parse_pack(argc, argv, 2)).pack();
    } catch (const Error& error) {
        fail(error.what());
    }
}

int Application::runCli(const std::vector<std::string>& args) const {
#if defined(AEGIS_ENABLE_DBUS)
    Cli client;
    return client.run(args);
#else
    (void)args;
    throw std::runtime_error("This build only supports 'bundle create'");
#endif
}

int Application::runDaemon(const std::vector<std::string>& args) const {
#if !defined(AEGIS_ENABLE_DBUS)
    (void)args;
    throw std::runtime_error("Daemon support is disabled in this build");
#else
    AppLog::Init(AppLog::Level::debug, nullptr, "aegis-daemon");
    LOG_I("Starting aegis daemon");

    OtaService service = make_ota_service(args);
    service.resumeAfterBoot();

    const SignalSet signal_set = make_daemon_signal_set();
    DbusService dbus(service);
    start_daemon_signal_forwarder(dbus, signal_set);
    dbus.run();
    return 0;
#endif
}

int Application::run(int argc, char** argv) {
    const auto args = toArgs(argc, argv);
    if (args.empty()) {
        return runCli(args);
    }

    if (args[0] == "pack") {
        return runPack(argc, argv);
    }

    if (args[0] == "daemon") {
        return runDaemon(args);
    }

    return runCli(args);
}

}  // namespace aegis
