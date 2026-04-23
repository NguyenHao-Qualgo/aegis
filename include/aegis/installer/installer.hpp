#pragma once

#include <stop_token>

#include "aegis/core/types.hpp"
#include "aegis/core/ota_state_machine.hpp"
#include "aegis/installer/archive_handler.hpp"
#include "aegis/installer/raw_handler.hpp"

namespace aegis {

class PackageInstaller {
public:
    explicit PackageInstaller(const InstallOptions &options);
    int install(OtaStateMachine& machine, std::stop_token stop = {});

private:
    const InstallOptions &options_;
    RawHandler     raw_handler_;
    ArchiveHandler archive_handler_;
};

}  // namespace aegis
