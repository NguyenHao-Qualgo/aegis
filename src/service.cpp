#include "rauc/service.h"
#include "rauc/context.h"
#include "rauc/install.h"
#include "rauc/mark.h"
#include "rauc/utils.h"

#include <atomic>
#include <csignal>
#include <thread>

// NOTE: A full D-Bus service requires either sdbus-c++, dbus-cxx, or raw
// libdbus. This is a skeleton showing the intended structure. Replace the
// GMainLoop polling with your preferred D-Bus binding.

namespace rauc {

static std::atomic<bool> g_running{false};

static void signal_handler(int /*sig*/) {
    g_running = false;
}

Result<void> service_run() {
    auto& ctx = Context::instance();
    if (!ctx.is_initialized())
        return Result<void>::err("Context not initialized");

    LOG_INFO("Starting RAUC service (compatible=%s, bootloader=%s)",
             ctx.config().compatible.c_str(),
             to_string(ctx.config().bootloader));

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    g_running = true;

    // TODO: Replace with actual D-Bus registration:
    //   - Register de.pengutronix.rauc.Installer interface
    //   - Expose methods: Install, InstallBundle, Info, Mark, GetSlotStatus
    //   - Expose properties: Operation, LastError, Progress, Compatible, BootSlot
    //   - Expose signal: Completed
    //
    // Example with sdbus-c++:
    //   auto conn = sdbus::createSystemBusConnection("de.pengutronix.rauc");
    //   auto obj  = sdbus::createObject(*conn, "/");
    //   obj->registerMethod("Install").onInterface("de.pengutronix.rauc.Installer")
    //       .implementedAs([](const std::string& source) { ... });
    //   conn->enterEventLoop();

    LOG_INFO("RAUC service running (waiting for D-Bus requests)...");

    // Auto-install check
    if (!ctx.config().autoinstall_path.empty() &&
        path_exists(ctx.config().autoinstall_path)) {
        LOG_INFO("Auto-install bundle found: %s", ctx.config().autoinstall_path.c_str());
        InstallArgs args;
        args.name = ctx.config().autoinstall_path;
        auto res = install_bundle(ctx.config().autoinstall_path, args);
        if (!res) LOG_ERROR("Auto-install failed: %s", res.error().c_str());
    }

    // Main event loop placeholder
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("RAUC service stopped");
    return Result<void>::ok();
}

void service_stop() {
    g_running = false;
}

} // namespace rauc
