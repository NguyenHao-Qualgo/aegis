#include "aegis/dbus_service.hpp"
#include "aegis/util.hpp"

#if __has_include(<sdbus-c++/VTableItems.h>)
#include <sdbus-c++/VTableItems.h>
#endif

namespace aegis {

namespace {
const sdbus::BusName kServiceName{"de.skytrack.Aegis"};
const sdbus::InterfaceName kInterfaceName{"de.skytrack.Aegis1"};
const sdbus::ObjectPath kObjectPath{"/de/skytrack/Aegis"};
}

DbusService::DbusService(OtaService& service)
    : service_(service),
      connection_(sdbus::createSystemBusConnection(kServiceName)),
      object_(sdbus::createObject(*connection_, kObjectPath)) {
    object_->addVTable(
        kInterfaceName,
        sdbus::registerMethod("Install")
            .withInputParamNames("bundle")
            .implementedAs([this](const std::string& bundle) {
                logInfo("DBus Install called");
                service_.startInstall(bundle);
                logInfo("DBus Install returned");
            }),
        sdbus::registerMethod("GetStatus")
            .withOutputParamNames("status")
            .implementedAs([this]() {
                return toMap(service_.getStatus());
            }),
        sdbus::registerMethod("MarkGood")
            .implementedAs([this]() {
                service_.markGood();
            }),
        sdbus::registerMethod("MarkBad")
            .implementedAs([this]() {
                service_.markBad();
            }),
        sdbus::registerMethod("MarkActive")
            .withInputParamNames("slot")
            .implementedAs([this](const std::string& slot) {
                service_.markActive(slot);
            }),
        sdbus::registerMethod("GetPrimary")
            .withOutputParamNames("slot")
            .implementedAs([this]() {
                return service_.getPrimary();
            }),
        sdbus::registerMethod("GetBooted")
            .withOutputParamNames("slot")
            .implementedAs([this]() {
                return service_.getBooted();
            }),
        sdbus::registerSignal("StatusChanged")
            .withParameters<std::map<std::string, sdbus::Variant>>("status")
    );

    service_.setStatusChangedCallback([this](const OtaStatus& status) {
        emitStatusChanged(status);
    });
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

void DbusService::emitStatusChanged(const OtaStatus& status) {
    const sdbus::SignalName kStatusChangedSignal{"StatusChanged"};
    auto signal = object_->createSignal(kInterfaceName, kStatusChangedSignal);
    signal << toMap(status);
    object_->emitSignal(signal);
}

void DbusService::run() {
    connection_->enterEventLoop();
}

}  // namespace aegis