#pragma once

#include "aegis/commands.h"

#include <string>

namespace aegis {

class CommandRegistry {
public:
    CommandRegistry();

    ICommand* find(const std::string& name);

private:
    CommandPtr bundle_;
    CommandPtr install_;
    CommandPtr info_;
    CommandPtr status_;
    CommandPtr mark_;
    CommandPtr extract_;
    CommandPtr resign_;
    CommandPtr service_;
    CommandPtr mount_;
    CommandPtr version_;
};

} // namespace aegis