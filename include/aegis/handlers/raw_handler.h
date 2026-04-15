#pragma once

#include "aegis/handlers/block_handler.h"

namespace aegis {

/// Direct writer for raw-like slots such as raw, nand, nor, and boot partitions.
class RawSlotUpdateHandler : public BlockDeviceUpdateHandler {
  public:
    const char* name() const override {
        return "raw-slot";
    }

  protected:
    const char* target_kind() const override {
        return "raw image";
    }
};

} // namespace aegis
