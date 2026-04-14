#include "aegis/cli/application.h"

#include "aegis/cli/cli_parser.h"
#include "aegis/cli/command_registry.h"

#include <iostream>

namespace aegis {

int Application::run(int argc, char* argv[]) {
    if (argc < 2) {
        CliParser::print_usage();
        return 1;
    }

    CliParser parser;
    ParseResult parsed = parser.parse(argc, argv);

    if (parsed.action == ParseAction::ExitSuccess) {
        return 0;
    }

    if (parsed.options.command.empty()) {
        std::cerr << "Error: missing command\n";
        CliParser::print_usage();
        return 1;
    }

    CommandRegistry registry;
    ICommand* command = registry.find(parsed.options.command);
    if (!command) {
        std::cerr << "Unknown command: " << parsed.options.command << "\n";
        CliParser::print_usage();
        return 1;
    }

    return command->execute(parsed.options);
}

} // namespace aegis
