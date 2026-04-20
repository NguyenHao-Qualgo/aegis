#pragma once

#include <cstdint>
#include <string>

namespace aegis {

class BundleExtractor {
public:
    // Extract all entries from the bundle tar.gz into destDir.
    void extract(const std::string& bundlePath, std::uint64_t payloadSize,
                 const std::string& destDir) const;

    // Extract a single named entry from the bundle into destDir (fast; skips all other entries).
    void extractEntry(const std::string& bundlePath, std::uint64_t payloadSize,
                      const std::string& entryName, const std::string& destDir) const;

    // Stream a named entry directly to destPath (e.g. a block device).
    // If expectedSha256 is non-empty, verifies the SHA-256 of all bytes (including
    // sparse zero-filled gaps) matches before returning; throws on mismatch.
    void streamEntry(const std::string& bundlePath, std::uint64_t payloadSize,
                     const std::string& entryName, const std::string& destPath,
                     const std::string& expectedSha256 = {}) const;
};

}  // namespace aegis
