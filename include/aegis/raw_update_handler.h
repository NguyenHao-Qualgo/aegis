#pragma once

#include "aegis/update_handler.h"

namespace aegis {

/// Raw dd-style image writer (for type=raw, nand, nor, boot-emmc, etc.)
class RawUpdateHandler : public IUpdateHandler {
public:
    Result<void> install(const std::string& image_path,
                         const ManifestImage& image,
                         Slot& target_slot,
                         ProgressCallback progress = {}) override;
};

} // namespace aegis
