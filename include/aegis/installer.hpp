#pragma once

#include "aegis/handlers.hpp"
#include "aegis/types.hpp"

namespace aegis {

class PackageInstaller {
public:
    explicit PackageInstaller(const InstallOptions &options);
    int install();

private:
    const InstallOptions &options_;
    RawHandler     raw_handler_;
    ArchiveHandler archive_handler_;
};

}  // namespace aegis
