#pragma once

#include "aegis/error.h"

#include <string>

namespace aegis {

Result<void> service_run();

void service_stop();

} // namespace aegis
