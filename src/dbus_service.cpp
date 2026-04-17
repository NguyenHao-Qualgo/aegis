#include "aegis/dbus_service.hpp"

namespace aegis {

DbusService::DbusService(OtaService& service)
    : service_(service),
      connection_(sdbus::createSystemBusConnection("de.skytrack.Aegis")),
      object_(sdbus::createObject(*connection_, "/de/skytrack/Aegis")) {
    object_->registerMethod("Install").onInterface("de.skytrack.Aegis1").implementedAs([this](const std::string& bundle) {
        service_.install(bundle);
    });
    object_->registerMethod("GetStatus").onInterface("de.skytrack.Aegis1").implementedAs([this]() {
        return toMap(service_.getStatus());
    });
    object_->registerMethod("MarkGood").onInterface("de.skytrack.Aegis1").implementedAs([this]() {
        service_.markGood();
    });
    object_->registerMethod("MarkBad").onInterface("de.skytrack.Aegis1").implementedAs([this]() {
        service_.markBad();
    });
    object_->registerMethod("MarkActive").onInterface("de.skytrack.Aegis1").implementedAs([this](const std::string& slot) {
        service_.markActive(slot);
    });
    object_->registerMethod("GetPrimary").onInterface("de.skytrack.Aegis1").implementedAs([this]() {
        return service_.getPrimary();
    });
    object_->registerMethod("GetBooted").onInterface("de.skytrack.Aegis1").implementedAs([this]() {
        return service_.getBooted();
    });
    object_->finishRegistration();
}

std::map<std::string, sdbus::Variant> DbusService::toMap(const OtaStatus& status) const {
    return {
        {"State", sdbus::Variant(toString(status.state))},
        {"Operation", sdbus::Variant(status.operation)},
        {"Progress", sdbus::Variant(static_cast<int32_t>(status.progress))},
        {"Message", sdbus::Variant(status.message)},
        {"LastError", sdbus::Variant(status.lastError)},
        {"BootedSlot", sdbus::Variant(status.bootedSlot)},
        {"PrimarySlot", sdbus::Variant(status.primarySlot)},
        {"TargetSlot", sdbus::Variant(status.targetSlot ? *status.targetSlot : std::string{})},
        {"BundleVersion", sdbus::Variant(status.bundleVersion)}
    };
}

void DbusService::run() {
    connection_->enterEventLoop();
}

}  // namespace aegis
