#include "aegis/stub/gcs_stub.hpp"

#include <iostream>

#include "aegis/core/types.hpp"
#include "aegis/common/logging.hpp"

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
    LOG_I("[gcs-stub] state={} op={} progress={}%", toString(status.state), status.operation, status.progress);
    if (!status.message.empty()) LOG_I(" msg={}", status.message);
    if (!status.lastError.empty()) LOG_I(" error={}", status.lastError);
    LOG_I("");
}

}  // namespace aegis
