#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "aegis/core/types.hpp"
#include "aegis/common/util.hpp"

namespace aegis {

inline bool to_bool(const std::string &value) {
    const std::string v = trim(value);
    return v == "true" || v == "\"true\"";
}

struct ManifestParseResult {
    std::vector<ManifestEntry> entries;
    std::string hw_compatibility;
};

ManifestParseResult parse_manifest(const std::string &sw_description, const std::string &target_slot = {});
ManifestEntry             *find_manifest_entry(std::vector<ManifestEntry> &entries, const std::string &name);
AesMaterial                parse_aes_key_file(const std::string &path);

}  // namespace aegis
