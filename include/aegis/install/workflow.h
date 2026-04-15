#pragma once

#include "aegis/bootchooser.h"
#include "aegis/bundle.h"
#include "aegis/install.h"

#include <map>

namespace aegis {

class InstallerWorkflow {
  public:
    explicit InstallerWorkflow(InstallArgs& args);

    Result<void> install(const std::string& bundle_path);

  private:
    Result<void> open_bundle(const std::string& bundle_path);
    Result<void> verify_compatibility() const;
    Result<void> determine_install_plans();
    Result<void> check_slot_devices() const;
    Result<void> run_hook(const std::string& handler_path, const std::string& action) const;
    void deactivate_target_slots();
    Result<void> install_images();
    void update_slot_status(const InstallPlan& plan) const;
    Result<void> activate_installed_slots();
    void save_status() const;
    void notify(const std::string& message) const;

  private:
    InstallArgs& args_;
    Bundle bundle_;
    std::map<std::string, Slot*> target_group_;
    std::vector<InstallPlan> plans_;
    std::unique_ptr<IBootchooser> bootchooser_;
};

} // namespace aegis
