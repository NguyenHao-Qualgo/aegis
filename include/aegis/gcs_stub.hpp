#pragma once

#include <optional>

#include "aegis/gcs_client.hpp"

namespace aegis {

// Stub GCS client for development and testing.
// Returns no update by default; set pendingUpdate to inject one.
class GcsStub : public IGcsClient {
public:
    std::optional<GcsUpdateInfo> pendingUpdate;

    std::optional<GcsUpdateInfo> checkForUpdate() override;
    void reportStatus(const OtaStatus& status) override;
};

}  // namespace aegis
