#include "aegis/install/hook_runner.h"

#include "aegis/utils.h"

namespace aegis {

std::vector<std::string> ShellHookRunner::build_env(const SystemConfig& config,
                                                    const Bundle& bundle,
                                                    const std::string& mount_prefix) {
    return {
        "AEGIS_SYSTEM_COMPATIBLE=" + config.compatible,
        "AEGIS_MOUNT_PREFIX=" + mount_prefix,
        "AEGIS_BUNDLE_MOUNT_POINT=" + bundle.mount_point,
        "AEGIS_UPDATE_SOURCE=" + bundle.mount_point,
        "AEGIS_BUNDLE_COMPATIBLE=" + bundle.manifest.compatible,
        "AEGIS_BUNDLE_VERSION=" + bundle.manifest.version,
    };
}

Result<void> ShellHookRunner::run(const std::string& handler_path,
                                  const std::string& action,
                                  const SystemConfig& config,
                                  const Bundle& bundle,
                                  const std::string& mount_prefix) const {
    if (handler_path.empty()) {
        LOG_DEBUG("No handler configured for action '%s'", action.c_str());
        return Result<void>::ok();
    }
    if (!path_exists(handler_path)) {
        LOG_WARNING("Handler not found: %s", handler_path.c_str());
        return Result<void>::ok();
    }

    LOG_INFO("Running hook '%s' via %s", action.c_str(), handler_path.c_str());
    auto result = run_command({handler_path, action},
                              build_env(config, bundle, mount_prefix));
    if (result.first != 0) {
        return Result<void>::err("Handler '" + action + "' failed (exit " +
                                 std::to_string(result.first) + "): " + result.second);
    }

    return Result<void>::ok();
}

} // namespace aegis
