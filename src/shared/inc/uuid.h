#pragma once

#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace Uuid {

inline std::string New() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    uint32_t data[4];
    for (int i = 0; i < 4; ++i)
        data[i] = dis(gen);
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << data[0] << "-" << std::setw(4) << ((data[1] >> 16) & 0xFFFF)
       << "-" << std::setw(4) << (((data[1] >> 0) & 0x0FFF) | 0x4000) << "-"  // version 4
       << std::setw(4) << (((data[2] >> 16) & 0x3FFF) | 0x8000) << "-"        // variant 1
       << std::setw(4) << (data[2] & 0xFFFF) << std::setw(8) << data[3];
    return ss.str();
}

class Uuid {
    std::string value;

   public:
    Uuid() : value(New()) {
    }
    explicit Uuid(const std::string& v) : value(v) {
    }
    Uuid& operator=(const std::string& v) {
        value = v;
        return *this;
    }
    operator std::string() const {
        return value;
    }
    const std::string& str() const {
        return value;
    }
};

}  // namespace Uuid