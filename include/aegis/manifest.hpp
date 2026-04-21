#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "aegis/types.hpp"

namespace aegis {

inline std::string trim(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

inline bool to_bool(const std::string &value) {
    const std::string v = trim(value);
    return v == "true" || v == "\"true\"";
}

inline std::string strip_quotes(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::vector<ManifestEntry> parse_manifest(const std::string &sw_description, const std::string &target_slot = {});
ManifestEntry             *find_manifest_entry(std::vector<ManifestEntry> &entries, const std::string &name);
AesMaterial                parse_aes_key_file(const std::string &path);

}  // namespace aegis
