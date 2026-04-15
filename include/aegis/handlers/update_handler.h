#pragma once

#include "aegis/error.h"
#include "aegis/manifest.h"
#include "aegis/slot.h"

#include <functional>
#include <string>

namespace aegis {

/// Progress callback: (percentage, message)
using ProgressCallback = std::function<void(int, const std::string&)>;

enum class UpdatePayloadKind {
    BlockImage,
    Archive,
};

/// Interface for slot update handlers
class UpdateHandler {
  public:
    virtual ~UpdateHandler() = default;

    virtual Result<void> install(const std::string& image_path, const ManifestImage& image,
                                 Slot& target_slot, ProgressCallback progress = {}) = 0;

    [[nodiscard]] virtual const char* name() const = 0;
};

class UpdateHandlerFactory {
  public:
    static UpdatePayloadKind classify_payload(const std::string& filename);
    static std::unique_ptr<UpdateHandler> create(SlotType slot_type,
                                                 UpdatePayloadKind payload_kind);
};

/// Write raw image data from source_path to device_path with progress
Result<void> write_image_to_device(const std::string& source_path, const std::string& device_path,
                                   ProgressCallback progress = {});

} // namespace aegis
