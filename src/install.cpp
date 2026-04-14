#include "aegis/install.h"
#include "aegis/bootchooser.h"
#include "aegis/checksum.h"
#include "aegis/context.h"
#include "aegis/status_file.h"
#include "aegis/utils.h"

namespace aegis {

namespace {

bool has_suffix(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_archive_payload(const std::string& filename) {
    return has_suffix(filename, ".tar") ||
           has_suffix(filename, ".tar.gz") ||
           has_suffix(filename, ".tgz") ||
           has_suffix(filename, ".tar.xz") ||
           has_suffix(filename, ".txz") ||
           has_suffix(filename, ".tar.bz2") ||
           has_suffix(filename, ".tbz2");
}

} // namespace

Result<std::vector<InstallPlan>> make_install_plans(
    const Manifest& manifest,
    std::map<std::string, Slot*>& target_group) {

    std::vector<InstallPlan> plans;

    for (auto& image : manifest.images) {
        auto it = target_group.find(image.slotclass);
        if (it == target_group.end()) {
            return Result<std::vector<InstallPlan>>::err(
                "No target slot for class '" + image.slotclass +
                "' of image '" + image.filename + "'");
        }

        if (it->second->readonly) {
            return Result<std::vector<InstallPlan>>::err(
                "Target slot for class '" + image.slotclass + "' is readonly");
        }

        InstallPlan plan;
        plan.image       = &image;
        plan.target_slot = it->second;
        plans.push_back(plan);

        LOG_INFO("Plan: %s -> %s (%s)",
                 image.filename.c_str(),
                 it->second->name.c_str(),
                 it->second->device.c_str());
    }

    return Result<std::vector<InstallPlan>>::ok(std::move(plans));
}

static Result<void> run_handler(const std::string& handler_path,
                                 const std::string& action,
                                 const Manifest& manifest,
                                 const std::string& bundle_mount_point) {
    if (handler_path.empty()) return Result<void>::ok();
    if (!path_exists(handler_path)) {
        LOG_WARNING("Handler not found: %s", handler_path.c_str());
        return Result<void>::ok();
    }

    std::vector<std::string> env = {
        "AEGIS_SYSTEM_COMPATIBLE=" + Context::instance().config().compatible,
        "AEGIS_MOUNT_PREFIX=" + Context::instance().mount_prefix(),
        "AEGIS_BUNDLE_MOUNT_POINT=" + bundle_mount_point,
        "AEGIS_UPDATE_SOURCE=" + bundle_mount_point,
        "AEGIS_BUNDLE_COMPATIBLE=" + manifest.compatible,
        "AEGIS_BUNDLE_VERSION=" + manifest.version,
    };

    auto res = run_command({handler_path, action}, env);
    if (res.exit_code != 0)
        return Result<void>::err("Handler '" + action + "' failed (exit "
                                 + std::to_string(res.exit_code) + "): "
                                 + res.stderr_str);
    return Result<void>::ok();
}

static Result<void> check_compatible(const Manifest& manifest,
                                      const SystemConfig& config,
                                      bool ignore) {
    if (ignore) {
        LOG_WARNING("Ignoring compatible check (forced)");
        return Result<void>::ok();
    }
    if (manifest.compatible != config.compatible) {
        return Result<void>::err(
            "Bundle compatible '" + manifest.compatible +
            "' does not match system compatible '" + config.compatible + "'");
    }
    return Result<void>::ok();
}

static Result<void> check_slot_devices(const std::vector<InstallPlan>& plans) {
    for (auto& plan : plans) {
        if (!path_exists(plan.target_slot->device)) {
            return Result<void>::err(
                "Destination device '" + plan.target_slot->device +
                "' for slot '" + plan.target_slot->name + "' not found");
        }
    }
    return Result<void>::ok();
}

Result<void> install_bundle(const std::string& bundle_path,
                            InstallArgs& args) {
    auto& ctx = Context::instance();
    auto& config = ctx.config();

    auto notify = [&](const std::string& msg) {
        LOG_INFO("%s", msg.c_str());
        if (args.status_notify) args.status_notify(msg);
    };

    notify("Opening bundle " + bundle_path);

    // 1. Open and verify bundle
    SigningParams verify_params;
    verify_params.keyring_path        = ctx.keyring_path();
    verify_params.allow_partial_chain = config.keyring_allow_partial_chain;
    verify_params.check_crl           = config.keyring_check_crl;

    auto bundle_result = bundle_open(bundle_path, verify_params);
    if (!bundle_result) return Result<void>::err(bundle_result.error());
    auto& bundle = bundle_result.value();

    // 2. Check compatibility
    notify("Checking compatibility");
    auto compat = check_compatible(bundle.manifest, config, args.ignore_compatible);
    if (!compat) return compat;

    // 3. Mount the bundle
    notify("Mounting bundle");
    auto mount_res = bundle_mount(bundle);
    if (!mount_res) return mount_res;

    // Ensure cleanup on any exit path
    struct BundleGuard {
        Bundle& b;
        ~BundleGuard() { bundle_unmount(b); }
    } guard{bundle};

    // 4. Determine target slots
    notify("Determining target slots");
    auto target_group = get_target_group(config.slots, ctx.boot_slot());
    if (target_group.empty())
        return Result<void>::err("No inactive target slots available");

    // 5. Build install plans
    auto plans_result = make_install_plans(bundle.manifest, target_group);
    if (!plans_result) return Result<void>::err(plans_result.error());
    auto& plans = plans_result.value();

    // 6. Pre-install checks
    auto dev_check = check_slot_devices(plans);
    if (!dev_check) return dev_check;

    // 7. Run pre-install handler
    notify("Running pre-install handler");
    auto pre_res = run_handler(config.handler_pre_install, "pre-install",
                                bundle.manifest, bundle.mount_point);
    if (!pre_res) return pre_res;

    // 8. Create bootchooser and deactivate target slots
    auto bootchooser = create_bootchooser(config);

    for (auto& plan : plans) {
        if (!plan.target_slot->bootname.empty()) {
            notify("Deactivating slot " + plan.target_slot->name);
            bootchooser->set_state(*plan.target_slot, false);
        }
    }

    // 9. Install each image
    int total = static_cast<int>(plans.size());
    int current = 0;
    for (auto& plan : plans) {
        current++;
        notify("Installing image " + std::to_string(current) + "/" +
               std::to_string(total) + ": " + plan.image->filename +
               " -> " + plan.target_slot->name);

        std::string image_path = bundle.mount_point + "/" + plan.image->filename;

        auto handler = create_update_handler(plan.target_slot->type,
                                             is_archive_payload(plan.image->filename));
        auto install_res = handler->install(image_path, *plan.image,
                                             *plan.target_slot, args.progress);
        if (!install_res) {
            return Result<void>::err("Failed to install " + plan.image->filename +
                                     " to " + plan.target_slot->name +
                                     ": " + install_res.error());
        }

        // 10. Update slot status
        auto& slot_status = plan.target_slot->status;
        slot_status.bundle_compatible  = bundle.manifest.compatible;
        slot_status.bundle_version     = bundle.manifest.version;
        slot_status.bundle_description = bundle.manifest.description;
        slot_status.bundle_build       = bundle.manifest.build;
        slot_status.checksum_sha256    = plan.image->sha256;
        slot_status.checksum_size      = plan.image->size;
        slot_status.installed_timestamp = current_timestamp();
        slot_status.installed_count++;
        slot_status.status = "ok";

        if (!config.data_directory.empty()) {
            save_slot_status(*plan.target_slot, config.data_directory);
        }
    }

    // 11. Activate the target group in the bootloader
    if (config.activate_installed) {
        for (auto& plan : plans) {
            if (!plan.target_slot->bootname.empty()) {
                notify("Activating slot " + plan.target_slot->name);
                auto act_res = bootchooser->set_primary(*plan.target_slot);
                if (!act_res)
                    return Result<void>::err("Failed to activate " +
                                             plan.target_slot->name + ": " +
                                             act_res.error());
                bootchooser->set_state(*plan.target_slot, true);

                plan.target_slot->status.activated_timestamp = current_timestamp();
                plan.target_slot->status.activated_count++;
            }
        }
    }

    // 12. Save status
    if (!config.statusfile.empty()) {
        save_all_slot_status(config.slots, config.statusfile);
    }

    // 13. Run post-install handler
    notify("Running post-install handler");
    auto post_res = run_handler(config.handler_post_install, "post-install",
                                 bundle.manifest, bundle.mount_point);
    if (!post_res)
        LOG_WARNING("Post-install handler failed: %s", post_res.error().c_str());

    notify("Installation complete");
    return Result<void>::ok();
}

} // namespace aegis
