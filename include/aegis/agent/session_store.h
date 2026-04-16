#pragma once

#include "aegis/agent/session.h"
#include "aegis/config_file.h"
#include "aegis/error.h"

#include <string>

namespace aegis {

class OtaSessionStore {
  public:
    explicit OtaSessionStore(const SystemConfig& config);

    Result<OtaSession> load() const;
    Result<void> save(const OtaSession& session) const;
    Result<void> clear() const;

    const std::string& path() const;

    static std::string default_path(const SystemConfig& config);

  private:
    std::string path_;
};

} // namespace aegis