#pragma once

#include "rauc/error.h"

#include <string>

namespace rauc {

/// Mark the booted slot as good
Result<void> mark_good(const std::string& slot_identifier = {});

/// Mark a slot as bad (disable it from boot selection)
Result<void> mark_bad(const std::string& slot_identifier = {});

/// Mark a slot as active (set it as next boot target)
Result<void> mark_active(const std::string& slot_identifier);

} // namespace rauc
