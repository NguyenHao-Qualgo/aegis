#pragma once

#include "aegis/bundle.h"
#include "aegis/error.h"
#include "aegis/handlers/update_handler.h"
#include "aegis/manifest.h"
#include "aegis/slot.h"

#include <functional>
#include <string>
#include <vector>

namespace aegis {

/// A single image-to-slot mapping
struct InstallPlan {
    const ManifestImage* image = nullptr;
    Slot* target_slot = nullptr;
};

/// Installation arguments / options
struct InstallArgs {
    std::string name; ///< display name (bundle path)
    bool ignore_compatible = false;
    bool ignore_version_limit = false;
    std::string transaction_id;
    ProgressCallback progress;
    std::function<void(const std::string&)> status_notify;
};

/// Build install plans: map manifest images -> target slots
Result<std::vector<InstallPlan>> make_install_plans(const Manifest& manifest,
                                                    std::map<std::string, Slot*>& target_group);

/// Run the full installation process:
///   1. Open & verify bundle
///   2. Mount bundle squashfs
///   3. Check compatibility
///   4. Determine target slots
///   5. Run pre-install handlers/hooks
///   6. Write each image to its slot
///   7. Update slot status
///   8. Activate boot target
///   9. Run post-install handlers/hooks
///  10. Unmount & clean up
Result<void> install_bundle(const std::string& bundle_path, InstallArgs& args);

} // namespace aegis
