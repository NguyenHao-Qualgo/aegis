#pragma once

#include <map>
#include <memory>
#include <string>

#include <sdbus-c++/sdbus-c++.h>
#include <sdbus-c++/Types.h>

#include "aegis/ota_service.hpp"

namespace aegis {

class DbusService {
public:
    explicit DbusService(OtaService& service);
    void run();

private:
    std::map<std::string, sdbus::Variant> toMap(const OtaStatus& status) const;
    void emitStatusChanged(const OtaStatus& status);

    OtaService& service_;
    std::unique_ptr<sdbus::IConnection> connection_;
    std::unique_ptr<sdbus::IObject> object_;
};

}  // namespace aegis