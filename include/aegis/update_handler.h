#pragma once

#include "aegis/error.h"
#include "aegis/manifest.h"
#include "aegis/slot.h"

#include <functional>
#include <string>

namespace aegis {

/// Progress callback: (percentage, message)
using ProgressCallback = std::function<void(int, const std::string&)>;

/// Interface for slot update handlers
class IUpdateHandler {
public:
    virtual ~IUpdateHandler() = default;

    /// Write an image to a target slot
    virtual Result<void> install(const std::string& image_path,
                                 const ManifestImage& image,
                                 Slot& target_slot,
                                 ProgressCallback progress = {}) = 0;
};

/// Factory: select the correct handler for a given slot type
std::unique_ptr<IUpdateHandler> create_update_handler(SlotType type,
                                                      bool is_tar = false);

/// Write raw image data from source_path to device_path with progress
Result<void> write_image_to_device(const std::string& source_path,
                                   const std::string& device_path,
                                   ProgressCallback progress = {});

} // namespace aegis
