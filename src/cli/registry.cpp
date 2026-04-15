#include "aegis/cli/command_registry.h"
#include "aegis/cli/commands.h"

namespace aegis {

CommandRegistry::CommandRegistry() {
    register_command({"bundle"}, std::make_unique<BundleCommand>());
    register_command({"install"}, std::make_unique<InstallCommand>());
    register_command({"info"}, std::make_unique<InfoCommand>());
    register_command({"status"}, std::make_unique<StatusCommand>());
    register_command({"version"}, std::make_unique<VersionCommand>());
    register_command({"mark-good", "mark-bad", "mark-active"}, std::make_unique<MarkCommand>());
    register_command({"extract"}, std::make_unique<ExtractCommand>());
    register_command({"service"}, std::make_unique<ServiceCommand>());
    register_command({"mount"}, std::make_unique<MountCommand>());
}

void CommandRegistry::register_command(std::initializer_list<const char*> names,
                                       CommandPtr command) {
    ICommand* raw_command = command.get();
    owned_commands_.push_back(std::move(command));
    for (const char* name : names) {
        commands_.emplace(name, raw_command);
    }
}

ICommand* CommandRegistry::find(const std::string& name) {
    auto it = commands_.find(name);
    return it != commands_.end() ? it->second : nullptr;
}

} // namespace aegis
