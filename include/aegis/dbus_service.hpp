#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <sdbus-c++/sdbus-c++.h>
#include <sdbus-c++/Types.h>

#include "aegis/ota_service.hpp"

namespace aegis {

class DbusService {
public:
    explicit DbusService(OtaService& service);
    ~DbusService();

    void run();
    void stop();

private:
    std::map<std::string, sdbus::Variant> toMap(const OtaStatus& status) const;
    void signalLoop();

    OtaService& service_;
    std::unique_ptr<sdbus::IConnection> connection_;
    std::unique_ptr<sdbus::IObject> object_;

    std::mutex signalMutex_;
    std::condition_variable signalCv_;
    std::queue<std::map<std::string, sdbus::Variant>> signalQueue_;
    std::atomic<bool> stopSignals_{false};
    std::thread signalThread_;
};

}  // namespace aegis