#pragma once

#include <cstdint>
#include <string>

namespace aegis {

class BundleExtractor {
public:
    // Extract the first payloadSize bytes of bundlePath (a .tar.gz) into destDir.
    void extract(const std::string& bundlePath, const std::string& destDir,
                 std::uint64_t payloadSize) const;
};

}  // namespace aegis
