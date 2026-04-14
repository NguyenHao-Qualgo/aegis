#pragma once

#include "aegis/handlers/block_handler.h"

namespace aegis {

/// Direct writer for filesystem image payloads such as .ext4.
class FilesystemImageUpdateHandler : public BlockDeviceUpdateHandler {
public:
    const char* name() const override { return "filesystem-image"; }

protected:
    const char* target_kind() const override { return "filesystem image"; }
};

} // namespace aegis
