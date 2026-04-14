#pragma once

#include "aegis/update_handler.h"

namespace aegis {

/// Archive extraction handler for tar-based payloads such as .tar.gz.
class ArchiveUpdateHandler : public IUpdateHandler {
public:
    Result<void> install(const std::string& image_path,
                         const ManifestImage& image,
                         Slot& target_slot,
                         ProgressCallback progress = {}) override;
};

} // namespace aegis
