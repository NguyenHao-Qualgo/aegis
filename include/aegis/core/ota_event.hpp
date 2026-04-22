#pragma once

#include <stop_token>
#include <string>

namespace aegis {

struct OtaEvent {
    enum class Type {
        StartInstall,
        ResumeAfterBoot,
        MarkGood,
        MarkBad,
        Reset,
    };

    Type type {};
    std::string bundlePath;
    std::string message;
    std::stop_token stop;
};

}  // namespace aegis
