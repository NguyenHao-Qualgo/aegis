#pragma once

#include "aegis/error.h"

#include <string>

namespace aegis {

Result<void> mark_good(const std::string& slot_identifier = {});
Result<void> mark_bad(const std::string& slot_identifier = {});
Result<void> mark_active(const std::string& slot_identifier);

} // namespace aegis