#pragma once

#include "aegis/cli/cli_options.h"

namespace aegis {

class AppContext {
public:
    static void init_runtime(const CliOptions& opts);
    static void init_service(const CliOptions& opts);
};

} // namespace aegis
