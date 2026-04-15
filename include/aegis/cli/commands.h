#pragma once

#include "aegis/cli/cli_options.h"

#include <memory>
#include <string>

namespace aegis {

class ICommand {
  public:
    virtual ~ICommand() = default;
    virtual int execute(const CliOptions& opts) = 0;
};

using CommandPtr = std::unique_ptr<ICommand>;

class BundleCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class InstallCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class InfoCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class StatusCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class MarkCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class ExtractCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class ResignCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class ServiceCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class MountCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

class VersionCommand : public ICommand {
  public:
    int execute(const CliOptions& opts) override;
};

} // namespace aegis
