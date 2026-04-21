#include "aegis/service/gcs_stub.hpp"

#include <iostream>

#include "aegis/core/types.hpp"

namespace aegis {

std::optional<GcsUpdateInfo> GcsStub::checkForUpdate() {
    if (pendingUpdate) {
        auto update = *pendingUpdate;
        pendingUpdate.reset();
        return update;
    }
    return std::nullopt;
}

void GcsStub::reportStatus(const OtaStatus& status) {
    std::cout << "[gcs-stub] state=" << toString(status.state)
              << " op=" << status.operation
              << " progress=" << status.progress << "%";
    if (!status.message.empty()) std::cout << " msg=" << status.message;
    if (!status.lastError.empty()) std::cout << " error=" << status.lastError;
    std::cout << '\n';
}

}  // namespace aegis
