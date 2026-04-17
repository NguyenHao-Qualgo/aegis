#include "aegis/install/workflow.h"

#include "aegis/context.h"
#include "aegis/utils.h"

namespace aegis {

namespace {

class BundleMountGuard {
  public:
    explicit BundleMountGuard(Bundle& bundle) : bundle_(bundle) {}
    ~BundleMountGuard() {
        bundle_unmount(bundle_);
    }

  private:
    Bundle& bundle_;
};

Result<void> check_compatible(const Manifest& manifest, const SystemConfig& config, bool ignore) {
    if (ignore) {
        LOG_WARNING("Ignoring compatible check (forced)");
        return Result<void>::ok();
    }
    if (manifest.compatible != config.compatible) {
        return Result<void>::err("Bundle compatible '" + manifest.compatible +
                                 "' does not match system compatible '" + config.compatible + "'");
    }
    return Result<void>::ok();
}

} // namespace

InstallerWorkflow::InstallerWorkflow(InstallArgs& args) : args_(args) {
    auto& ctx = Context::instance();
    config_ = &ctx.config();
    bootchooser_ = &ctx.bootchooser();
    owned_status_store_ = std::make_unique<FileStatusStore>(ctx.config());
    owned_hook_runner_ = std::make_unique<ShellHookRunner>();
    status_store_ = owned_status_store_.get();
    hook_runner_ = owned_hook_runner_.get();
    boot_slot_ = ctx.boot_slot();
    keyring_path_ = ctx.keyring_path();
    mount_prefix_ = ctx.mount_prefix();
}

InstallerWorkflow::InstallerWorkflow(InstallArgs& args, SystemConfig& config,
                                     IBootchooser& bootchooser, IStatusStore& status_store,
                                     IHookRunner& hook_runner, std::string boot_slot,
                                     std::string keyring_path, std::string mount_prefix)
    : args_(args), config_(&config), bootchooser_(&bootchooser), status_store_(&status_store),
      hook_runner_(&hook_runner), boot_slot_(std::move(boot_slot)),
      keyring_path_(std::move(keyring_path)), mount_prefix_(std::move(mount_prefix)) {}

Result<void> InstallerWorkflow::install(const std::string& bundle_path) {
    finished_steps_.clear();
    current_step_active_ = false;

    begin_step("open-bundle", 5, "Opening bundle");
    auto open_result = open_bundle(bundle_path);
    if (!open_result) {
        return open_result;
    }
    finish_step("Bundle opened");

    BundleMountGuard guard(bundle_);

    begin_step("check-compatibility", 5, "Checking compatibility");
    auto compatibility = verify_compatibility();
    if (!compatibility) {
        return compatibility;
    }
    finish_step("Compatibility check passed");

    begin_step("determine-target-slots", 5, "Determining target slots");
    auto plan_result = determine_install_plans();
    if (!plan_result) {
        return plan_result;
    }
    finish_step("Target slots determined");

    begin_step("check-slot-devices", 5, "Checking slot devices");
    auto devices_ok = check_slot_devices();
    if (!devices_ok) {
        return devices_ok;
    }
    finish_step("Slot devices ready");

    begin_step("pre-install-hook", 5, "Running pre-install handler");
    auto pre_hook = hook_runner_->run(config_->handler_pre_install, "pre-install",
                                      *config_, bundle_, mount_prefix_);
    if (!pre_hook) {
        return pre_hook;
    }
    finish_step("Pre-install handler completed");

    begin_step("prepare-boot-state", 5, "Preparing boot state");
    auto deactivate_result = deactivate_target_slots();
    if (!deactivate_result) {
        return deactivate_result;
    }
    finish_step("Boot state prepared");

    begin_step("install-images", 55, "Installing images");
    auto install_result = install_images();
    if (!install_result) {
        return install_result;
    }
    finish_step("All images installed");

    begin_step("activate-installed-slots", 5, "Activating installed slots");
    auto activation_result = activate_installed_slots();
    if (!activation_result) {
        return activation_result;
    }
    finish_step("Installed slots activated");

    begin_step("save-status", 3, "Saving slot status");
    auto save_result = save_status();
    if (!save_result) {
        return save_result;
    }
    finish_step("Slot status saved");

    begin_step("post-install-hook", 2, "Running post-install handler");
    auto post_hook = hook_runner_->run(config_->handler_post_install, "post-install",
                                       *config_, bundle_, mount_prefix_);
    if (!post_hook) {
        LOG_WARNING("Post-install handler failed: %s", post_hook.error().c_str());
    }
    finish_step("Post-install handler completed");

    set_progress(100, "Installation complete");
    return Result<void>::ok();
}

Result<void> InstallerWorkflow::open_bundle(const std::string& bundle_path) {
    SigningParams verify_params;
    verify_params.keyring_path = keyring_path_;
    verify_params.allow_partial_chain = config_->keyring_allow_partial_chain;
    verify_params.check_crl = config_->keyring_check_crl;

    auto bundle_result = bundle_open(bundle_path, verify_params);
    if (!bundle_result) {
        return Result<void>::err(bundle_result.error());
    }

    bundle_ = std::move(bundle_result.value());

    update_step_progress(50, "Mounting bundle");
    auto mount_result = bundle_mount(bundle_);
    if (!mount_result) {
        return mount_result;
    }

    return Result<void>::ok();
}

Result<void> InstallerWorkflow::verify_compatibility() const {
    return check_compatible(bundle_.manifest, *config_, args_.ignore_compatible);
}

Result<void> InstallerWorkflow::determine_install_plans() {
    target_group_ = get_target_group(config_->slots, boot_slot_);
    if (target_group_.empty()) {
        return Result<void>::err("No inactive target slots available");
    }

    auto plans_result = make_install_plans(bundle_.manifest, target_group_);
    if (!plans_result) {
        return Result<void>::err(plans_result.error());
    }

    plans_ = std::move(plans_result.value());
    LOG_INFO("Determined %zu install plan(s)", plans_.size());
    return Result<void>::ok();
}

Result<void> InstallerWorkflow::check_slot_devices() const {
    for (const auto& plan : plans_) {
        LOG_DEBUG("Checking slot device: slot=%s device=%s",
                  plan.target_slot->name.c_str(),
                  plan.target_slot->device.c_str());

        if (!path_exists(plan.target_slot->device)) {
            return Result<void>::err("Destination device '" + plan.target_slot->device +
                                     "' for slot '" + plan.target_slot->name + "' not found");
        }
    }
    return Result<void>::ok();
}

Result<void> InstallerWorkflow::deactivate_target_slots() {
    for (auto& plan : plans_) {
        if (plan.target_slot->bootname.empty()) {
            continue;
        }
        notify("Deactivating slot " + plan.target_slot->name);
        auto res = bootchooser_->set_state(*plan.target_slot, false);
        if (!res) {
            return Result<void>::err("Failed to deactivate slot '" + plan.target_slot->name +
                                     "': " + res.error());
        }
    }
    return Result<void>::ok();
}

Result<void> InstallerWorkflow::install_images() {
    const int total = static_cast<int>(plans_.size());
    if (total <= 0) {
        update_step_progress(100, "No images to install");
        return Result<void>::ok();
    }

    for (int index = 0; index < total; ++index) {
        auto& plan = plans_[index];

        const int image_base = (index * 100) / total;
        const int image_next = ((index + 1) * 100) / total;
        const std::string label = std::to_string(index + 1) + "/" + std::to_string(total) +
                                  ": " + plan.image->filename + " -> " + plan.target_slot->name;

        update_step_progress(image_base, "Installing image " + label);

        auto mapped_progress = [this, image_base, image_next](int percent,
                                                              const std::string& message) {
            percent = std::max(0, std::min(100, percent));
            const int mapped = image_base + ((image_next - image_base) * percent) / 100;
            update_step_progress(mapped, message.empty() ? "Installing image" : message);
        };

        const std::string image_path = bundle_.mount_point + "/" + plan.image->filename;
        auto payload_kind = UpdateHandlerFactory::classify_payload(plan.image->filename);
        auto handler = UpdateHandlerFactory::create(plan.target_slot->type, payload_kind);

        auto install_result =
            handler->install(image_path, *plan.image, *plan.target_slot, mapped_progress);
        if (!install_result) {
            return Result<void>::err("Failed to install " + plan.image->filename + " to " +
                                     plan.target_slot->name + ": " + install_result.error());
        }

        auto update_result = update_slot_status(plan);
        if (!update_result) {
            return update_result;
        }
        update_step_progress(image_next, "Installed image " + label);
    }

    update_step_progress(100, "All images installed");
    return Result<void>::ok();
}

Result<void> InstallerWorkflow::update_slot_status(const InstallPlan& plan) const {
    auto& slot_status = plan.target_slot->status;

    slot_status.bundle_compatible = bundle_.manifest.compatible;
    slot_status.bundle_version = bundle_.manifest.version;
    slot_status.bundle_description = bundle_.manifest.description;
    slot_status.bundle_build = bundle_.manifest.build;
    slot_status.checksum_sha256 = plan.image->sha256;
    slot_status.checksum_size = plan.image->size;
    slot_status.installed_timestamp = current_timestamp();
    slot_status.installed_count++;

    return status_store_->save_slot(*plan.target_slot);
}

Result<void> InstallerWorkflow::activate_installed_slots() {
    if (!config_->activate_installed) {
        LOG_INFO("Activation of installed slots is disabled");
        return Result<void>::ok();
    }

    const int total = static_cast<int>(plans_.size());
    int index = 0;

    for (auto& plan : plans_) {
        if (plan.target_slot->bootname.empty()) {
            ++index;
            continue;
        }

        const int percent = total > 0 ? (index * 100) / total : 100;
        update_step_progress(percent, "Activating slot " + plan.target_slot->name);

        auto activate_result = bootchooser_->set_primary(*plan.target_slot);
        if (!activate_result) {
            return Result<void>::err("Failed to activate " + plan.target_slot->name + ": " +
                                     activate_result.error());
        }
        auto state_result = bootchooser_->set_state(*plan.target_slot, true);
        if (!state_result) {
            return Result<void>::err("Failed to mark slot bootable " + plan.target_slot->name + ": " +
                                     state_result.error());
        }

        plan.target_slot->status.activated_timestamp = current_timestamp();
        plan.target_slot->status.activated_count++;
        ++index;
    }

    return Result<void>::ok();
}

Result<void> InstallerWorkflow::save_status() const {
    return status_store_->save_all(config_->slots);
}

void InstallerWorkflow::notify(const std::string& message) const {
    LOG_INFO("%s", message.c_str());
    if (args_.status_notify) {
        args_.status_notify(message);
    }
}

void InstallerWorkflow::set_progress(int percent, const std::string& message) {
    LOG_INFO("progress: %d%% - %s", percent, message.c_str());

    if (args_.progress) {
        args_.progress(percent, message);
    }
    if (args_.status_notify) {
        args_.status_notify(message);
    }
}

int InstallerWorkflow::completed_weight() const {
    int sum = 0;
    for (const auto& step : finished_steps_) {
        sum += step.weight;
    }
    return sum;
}

void InstallerWorkflow::begin_step(const std::string& name, int weight, const std::string& message) {
    current_step_ = ProgressStep{name, weight};
    current_step_active_ = true;
    update_step_progress(0, message);
}

void InstallerWorkflow::update_step_progress(int sub_percent, const std::string& message) {
    if (!current_step_active_) {
        set_progress(completed_weight(), message);
        return;
    }

    sub_percent = std::max(0, std::min(100, sub_percent));
    const int base = completed_weight();
    const int mapped = base + (current_step_.weight * sub_percent) / 100;
    set_progress(mapped, message);
}

void InstallerWorkflow::finish_step(const std::string& message) {
    if (!current_step_active_) {
        return;
    }

    finished_steps_.push_back(current_step_);
    current_step_active_ = false;
    set_progress(completed_weight(), message);
}

} // namespace aegis
