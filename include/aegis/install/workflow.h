#pragma once

#include "aegis/bootchooser.h"
#include "aegis/bundle.h"
#include "aegis/install.h"

#include <map>

namespace aegis {

struct ProgressStep {
    std::string name;
    int weight = 0;
};

class InstallerWorkflow {
  public:
    explicit InstallerWorkflow(InstallArgs& args);

    Result<void> install(const std::string& bundle_path);

  private:
    Result<void> open_bundle(const std::string& bundle_path);
    [[nodiscard]] Result<void> verify_compatibility() const;
    Result<void> determine_install_plans();
    [[nodiscard]] Result<void> check_slot_devices() const;
    [[nodiscard]] Result<void> run_hook(const std::string& handler_path,
                                        const std::string& action) const;
    void deactivate_target_slots();
    Result<void> install_images();
    void update_slot_status(const InstallPlan& plan) const;
    Result<void> activate_installed_slots();
    void save_status() const;
    void notify(const std::string& message) const;
    void set_progress(int percent, const std::string& message);
	int completed_weight() const;
	int total_weight() const;
	void begin_step(const std::string& name, int weight, const std::string& message);
	void update_step_progress(int sub_percent, const std::string& message);
	void finish_step(const std::string& message);

    InstallArgs& args_;
    Bundle bundle_;
    std::map<std::string, Slot*> target_group_;
    std::vector<InstallPlan> plans_;
    std::unique_ptr<IBootchooser> bootchooser_;
	std::vector<ProgressStep> finished_steps_;
	ProgressStep current_step_;
	bool current_step_active_ = false;
};

} // namespace aegis
