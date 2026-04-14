#include "aegis/command_registry.h"
#include "aegis/commands.h"

namespace aegis {

CommandRegistry::CommandRegistry()
    : bundle_(std::make_unique<BundleCommand>()),
      install_(std::make_unique<InstallCommand>()),
      info_(std::make_unique<InfoCommand>()),
      status_(std::make_unique<StatusCommand>()),
      mark_(std::make_unique<MarkCommand>()),
      extract_(std::make_unique<ExtractCommand>()),
      resign_(std::make_unique<ResignCommand>()),
      service_(std::make_unique<ServiceCommand>()),
      mount_(std::make_unique<MountCommand>()) {}

ICommand* CommandRegistry::find(const std::string& name) {
    if (name == "bundle") return bundle_.get();
    if (name == "install") return install_.get();
    if (name == "info") return info_.get();
    if (name == "status") return status_.get();
    if (name == "mark-good" || name == "mark-bad" || name == "mark-active") return mark_.get();
    if (name == "extract") return extract_.get();
    if (name == "resign") return resign_.get();
    if (name == "service") return service_.get();
    if (name == "mount") return mount_.get();
    return nullptr;
}

} // namespace aegis