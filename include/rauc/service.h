#pragma once

#include "rauc/error.h"

#include <string>

namespace rauc {

Result<void> service_run();

void service_stop();

} // namespace rauc
