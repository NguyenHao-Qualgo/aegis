#pragma once

#include "aegis/bundle.h"
#include "aegis/config_file.h"
#include "aegis/error.h"

#include <string>
#include <vector>

namespace aegis {

class IHookRunner {
  public:
    virtual ~IHookRunner() = default;

    virtual Result<void> run(const std::string& handler_path,
                             const std::string& action,
                             const SystemConfig& config,
                             const Bundle& bundle,
                             const std::string& mount_prefix) const = 0;
};

class ShellHookRunner : public IHookRunner {
  public:
    Result<void> run(const std::string& handler_path,
                     const std::string& action,
                     const SystemConfig& config,
                     const Bundle& bundle,
                     const std::string& mount_prefix) const override;

  private:
    static std::vector<std::string> build_env(const SystemConfig& config,
                                              const Bundle& bundle,
                                              const std::string& mount_prefix);
};

} // namespace aegis
