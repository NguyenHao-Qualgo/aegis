#include "aegis/app.hpp"

#include <filesystem>
#include <iostream>
#include <syslog.h>
#include <vector>

#include "aegis/installer/archive_update_handler.hpp"
#include "aegis/bootloader/uboot_control.hpp"
#include "aegis/bundle/bundle_creator.hpp"
#include "aegis/bundle/bundle_verifier.hpp"
#include "aegis/cli.hpp"
#include "aegis/config.hpp"
#include "aegis/gcs_stub.hpp"
#include "aegis/ota_service.hpp"
#include "aegis/installer/raw_update_handler.hpp"
#include "aegis/state_store.hpp"
#include "aegis/util.hpp"

#if defined(AEGIS_ENABLE_DBUS)
#include "aegis/dbus_service.hpp"
#endif

namespace aegis {

namespace {
std::vector<std::string> toArgs(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return args;
}
}

int Application::run(int argc, char** argv) {
    const auto args = toArgs(argc, argv);
    if (args.empty()) {
#if defined(AEGIS_ENABLE_DBUS)
        Cli client;
        return client.run(args);
#else
        throw std::runtime_error("This build only supports 'bundle create'");
#endif
    }

    if (args[0] == "bundle") {
        if (args.size() >= 2 && args[1] == "create") {
            BundleCreateOptions options;
            options.compatible = getOptionValue(args, "--compatible");
            options.version = getOptionValue(args, "--version");
            options.outputBundle = getOptionValue(args, "--output");
            options.manifestPath = getOptionValue(args, "--manifest");
            options.sourceDirectory = getOptionValue(args, "--source-dir");
            options.certPath = getOptionValue(args, "--cert");
            options.keyPath = getOptionValue(args, "--key");
            const auto format = getOptionValue(args, "--format");
            if (!format.empty()) options.format = format;

            for (size_t i = 0; i < args.size(); ++i) {
                if (args[i] == "--artifact") {
                    if (i + 1 >= args.size()) {
                        throw std::runtime_error("--artifact requires a value");
                    }
                    options.artifacts.push_back(BundleCreator::parseArtifactSpec(args[i + 1]));
                    ++i;
                }
            }

            const bool usingManifest = !options.manifestPath.empty();
            if (usingManifest) {
                if (options.outputBundle.empty()) {
                    throw std::runtime_error("bundle create with --manifest requires --output");
                }
            } else if (options.compatible.empty() || options.version.empty() || options.outputBundle.empty() || options.artifacts.empty()) {
                throw std::runtime_error(
                    "bundle create requires either --manifest <manifest.ini> --output <bundle> "
                    "or --compatible --version --output and at least one --artifact <slot-class>:<type>:<path>");
            }
            BundleCreator creator(CommandRunner{});
            creator.create(options);
            std::cout << "Created bundle: " << options.outputBundle << '\n';
            return 0;
        }
        throw std::runtime_error("Unsupported bundle command");
    }

    if (args[0] == "daemon") {
#if !defined(AEGIS_ENABLE_DBUS)
        throw std::runtime_error("Daemon support is disabled in this build");
#else
        logInfo("Starting aegis daemon");

        const auto configPath = getOptionValue(args, "--config").empty() ? std::string("/etc/aegis/system.conf") : getOptionValue(args, "--config");
        logInfo("Loading config: " + configPath);
        ConfigLoader loader;
        const auto config = loader.load(configPath);
        std::filesystem::create_directories(config.dataDirectory);
        CommandRunner runner;
        auto bootControl = std::make_unique<UBootControl>(runner);
        auto verifier = std::make_unique<BundleVerifier>();
        std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers;
        updateHandlers.push_back(std::make_unique<ArchiveUpdateHandler>());
        updateHandlers.push_back(std::make_unique<RawUpdateHandler>());
        StateStore stateStore(joinPath(config.dataDirectory, "ota-state.env"));
        auto gcsClient = std::make_shared<GcsStub>();
        OtaService service(config, std::move(bootControl), std::move(verifier), std::move(updateHandlers), stateStore, std::move(gcsClient));
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
