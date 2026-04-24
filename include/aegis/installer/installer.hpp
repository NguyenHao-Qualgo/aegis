#pragma once

#include <memory>
#include <stop_token>
#include <string>
#include <unordered_map>

#include "aegis/core/types.hpp"
#include "aegis/core/ota_state_machine.hpp"
#include "aegis/installer/handler.hpp"

namespace aegis {

class PackageInstaller {
public:
    explicit PackageInstaller(const InstallOptions &options);
    ~PackageInstaller();
    int install(OtaStateMachine& machine, std::stop_token stop = {});

private:
    using HandlerMap = std::unordered_map<std::string, std::unique_ptr<IHandler>>;

    static HandlerMap createHandlers();
    IHandler& handlerFor(const std::string& type) const;

    const InstallOptions &options_;
    HandlerMap handlers_;
};

}  // namespace aegis
