#pragma once

#include "aegis/checksum.h"
#include "aegis/handlers/update_handler.h"

namespace aegis {

class BlockDeviceUpdateHandler : public UpdateHandler {
  public:
    Result<void> install(const std::string& image_path, const ManifestImage& image,
                         Slot& target_slot, ProgressCallback progress = {}) final;

  protected:
    virtual const char* target_kind() const = 0;
};

} // namespace aegis
