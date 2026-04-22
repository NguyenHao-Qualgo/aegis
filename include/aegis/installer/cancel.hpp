#pragma once

#include <stop_token>

namespace aegis {

class CancelSource {
public:
    std::stop_token token() const noexcept {
        return source_.get_token();
    }

    void request() noexcept {
        source_.request_stop();
    }

private:
    std::stop_source source_;
};

}  // namespace aegis
