#pragma once

#include "aegis/cli/commands.h"

#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace aegis {

class CommandRegistry {
  public:
    CommandRegistry();

    ICommand* find(const std::string& name);

  private:
    void register_command(std::initializer_list<const char*> names, CommandPtr command);

    std::vector<CommandPtr> owned_commands_;
    std::unordered_map<std::string, ICommand*> commands_;
};

} // namespace aegis
