#pragma once

#include <string>

#include "aegis/core/types.hpp"

namespace aegis {

class StateStore {
public:
    explicit StateStore(std::string path);

    OtaStatus load() const;
    void save(const OtaStatus& status) const;

private:
    std::string path_;
};

}  // namespace aegis
