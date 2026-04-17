#pragma once

#include <string>

#include "aegis/command_runner.hpp"
#include "aegis/types.hpp"

namespace aegis {

class Ext4Installer {
public:
    explicit Ext4Installer(CommandRunner runner);

    void installPayload(const std::string& payloadTarGz, const SlotConfig& slot, const std::string& workDir) const;

private:
    CommandRunner runner_;
};

}  // namespace aegis
