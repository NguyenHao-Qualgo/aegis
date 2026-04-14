#pragma once

#include "aegis/update_handler.h"

namespace aegis {

/// Direct block-image writer for filesystem images such as .ext4.
class FilesystemImageUpdateHandler : public IUpdateHandler {
public:
    Result<void> install(const std::string& image_path,
                         const ManifestImage& image,
                         Slot& target_slot,
                         ProgressCallback progress = {}) override;
};

} // namespace aegis
