#pragma once

#include "aegis/installer/handler.hpp"

namespace aegis {

class FileHandler final : public IHandler {
public:
    void install(const InstallContext& ctx,
                 StreamReader& reader,
                 const CpioEntry& cpio_entry,
                 const ManifestEntry& entry,
                 const AesMaterial* aes) override;
};

}  // namespace aegis
