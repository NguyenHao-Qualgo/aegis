#pragma once

#include "cli_options.h"

namespace aegis {

class CliParser {
public:
    ParseResult parse(int argc, char* argv[]) const;
    static void print_usage();
    static void print_version();
};

} // namespace aegis