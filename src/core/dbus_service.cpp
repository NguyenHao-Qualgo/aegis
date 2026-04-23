#include "aegis/core/dbus_service.hpp"
#include "aegis/core/types.hpp"
#include "aegis/common/util.hpp"

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
                LOG_I("DBus Install called");

                const auto state = service_.getStatus().state;
                if (state == OtaState::Download || state == OtaState::Install) {
                    throw sdbus::Error(sdbus::Error::Name{"de.skytrack.Aegis1.Error.Busy"},
                                       "Install already in progress");
                }

                service_.startInstall(bundle);

                LOG_I("DBus Install returned (async)");
            }),

        sdbus::registerMethod("Cancel")
            .implementedAs([this]() {
                LOG_I("DBus Cancel called");
                service_.cancelInstall();
            }),

        sdbus::registerMethod("GetStatus")
            .withOutputParamNames("status")
            .implementedAs([this]() {
                return toMap(service_.getStatus());
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

void DbusService::stop() {
    service_.cancelInstall();
    stopSignals_ = true;
    signalCv_.notify_all();
    if (connection_) {
        connection_->leaveEventLoop();
    }
}

}  // namespace aegis
