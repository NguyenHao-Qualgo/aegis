#include "aegis/dbus_service.hpp"
#include "aegis/util.hpp"

#include <thread>
#include <mutex>
#include <queue>

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

                std::thread([this, bundle]() {
                    try {
                        logInfo("Install thread started");
                        service_.startInstall(bundle);
                        logInfo("Install finished");
                    } catch (const std::exception& e) {
                        logError(std::string("Install failed: ") + e.what());
                    }
                }).detach();

                logInfo("DBus Install returned (async)");
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
        {
            std::lock_guard<std::mutex> lock(signalMutex_);
            signalQueue_.push(toMap(status));
        }
        signalCv_.notify_one();
    });

    signalThread_ = std::thread([this]() { signalLoop(); });
}

DbusService::~DbusService() {
    stopSignals_ = true;
    signalCv_.notify_all();
    if (signalThread_.joinable()) {
        signalThread_.join();
    }
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

void DbusService::signalLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(signalMutex_);
        signalCv_.wait(lock, [this]() {
            return !signalQueue_.empty() || stopSignals_.load();
        });

        if (stopSignals_ && signalQueue_.empty()) {
            break;
        }

        while (!signalQueue_.empty()) {
            auto map = std::move(signalQueue_.front());
            signalQueue_.pop();
            lock.unlock();

            auto signal = object_->createSignal(kInterfaceName, sdbus::SignalName{"StatusChanged"});
            signal << map;
            object_->emitSignal(signal);

            lock.lock();
        }
    }
}

void DbusService::run() {
    connection_->enterEventLoop();
}

}  // namespace aegis