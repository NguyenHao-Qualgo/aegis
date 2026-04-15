#include "aegis/install.h"

#include "aegis/context.h"
#include "aegis/install/workflow.h"
#include "aegis/utils.h"

namespace aegis {

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
        plan.image = &image;
        plan.target_slot = it->second;
        plans.push_back(plan);

        LOG_INFO("Plan: %s -> %s (%s)",
                 image.filename.c_str(),
                 it->second->name.c_str(),
                 it->second->device.c_str());
    }

    return Result<std::vector<InstallPlan>>::ok(std::move(plans));
}

Result<void> install_bundle(const std::string& bundle_path,
                            InstallArgs& args) {
    InstallerWorkflow workflow(args);
    return workflow.install(bundle_path);
}

} // namespace aegis
