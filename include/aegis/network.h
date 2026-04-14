#pragma once

#include "aegis/error.h"

#include <cstdint>
#include <functional>
#include <string>

namespace aegis {

using DownloadProgress = std::function<void(uint64_t, uint64_t)>;

/// Download a bundle from a URL to a local path
Result<void> download_bundle(const std::string& url,
                             const std::string& output_path,
                             uint64_t max_size = 0,
                             DownloadProgress progress = {});

Result<uint64_t> check_bundle_url(const std::string& url);

} // namespace aegis
