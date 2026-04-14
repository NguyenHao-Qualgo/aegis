#pragma once

#include "aegis/handlers/update_handler.h"

namespace aegis {

/// Extractor for mounted filesystem targets receiving tar-based payloads.
class MountedArchiveUpdateHandler : public UpdateHandler {
public:
    Result<void> install(const std::string& image_path,
                         const ManifestImage& image,
                         Slot& target_slot,
                         ProgressCallback progress = {}) override;

    const char* name() const override { return "mounted-archive"; }
};

} // namespace aegis
