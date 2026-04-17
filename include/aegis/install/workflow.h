#pragma once

#include "aegis/bootchooser.h"
#include "aegis/bundle.h"
#include "aegis/config_file.h"
#include "aegis/install.h"
#include "aegis/install/hook_runner.h"
#include "aegis/status_file.h"

#include <map>
#include <memory>

namespace aegis {

struct ProgressStep {
    std::string name;
    int weight = 0;
};

class InstallerWorkflow {
  public:
    explicit InstallerWorkflow(InstallArgs& args);
    InstallerWorkflow(InstallArgs& args, SystemConfig& config, IBootchooser& bootchooser,
                      IStatusStore& status_store, IHookRunner& hook_runner,
                      std::string boot_slot, std::string keyring_path, std::string mount_prefix);

    Result<void> install(const std::string& bundle_path);

  private:
    Result<void> open_bundle(const std::string& bundle_path);
    [[nodiscard]] Result<void> verify_compatibility() const;
    Result<void> determine_install_plans();
    [[nodiscard]] Result<void> check_slot_devices() const;
    Result<void> deactivate_target_slots();
    Result<void> install_images();
    Result<void> update_slot_status(const InstallPlan& plan) const;
    Result<void> activate_installed_slots();
    Result<void> save_status() const;
    void notify(const std::string& message) const;
    void set_progress(int percent, const std::string& message);
    int completed_weight() const;
    void begin_step(const std::string& name, int weight, const std::string& message);
    void update_step_progress(int sub_percent, const std::string& message);
    void finish_step(const std::string& message);

    InstallArgs& args_;
    SystemConfig* config_ = nullptr;
    IBootchooser* bootchooser_ = nullptr;
    IStatusStore* status_store_ = nullptr;
    std::unique_ptr<IStatusStore> owned_status_store_;
    std::unique_ptr<IHookRunner> owned_hook_runner_;
    IHookRunner* hook_runner_ = nullptr;
    std::string boot_slot_;
    std::string keyring_path_;
    std::string mount_prefix_;
    Bundle bundle_;
    std::map<std::string, Slot*> target_group_;
    std::vector<InstallPlan> plans_;
    std::vector<ProgressStep> finished_steps_;
    ProgressStep current_step_;
    bool current_step_active_ = false;
};

} // namespace aegis
