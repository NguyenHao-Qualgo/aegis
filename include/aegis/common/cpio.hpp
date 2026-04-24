#pragma once

#include "aegis/common/io.hpp"
#include "aegis/core/types.hpp"

namespace aegis {

void       skip_padding(StreamReader &reader, std::uint64_t size, std::size_t alignment = 4);
CpioEntry  read_cpio_entry(StreamReader &reader);

}  // namespace aegis
