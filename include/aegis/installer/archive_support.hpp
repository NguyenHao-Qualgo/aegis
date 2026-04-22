#pragma once

#include <filesystem>
#include <string>
#include <thread>

#include "aegis/core/types.hpp"

namespace aegis {

namespace fs = std::filesystem;

struct RestoreCwd {
    std::string old_cwd;
    bool active = false;

    ~RestoreCwd();
};

struct JoinThread {
    std::thread* thread = nullptr;

    ~JoinThread();
};

struct UnlinkPath {
    std::string path;

    ~UnlinkPath();
};

struct RemoveTree {
    fs::path path;

    ~RemoveTree();
};

struct ScopedMount {
    fs::path mountpoint;
    bool mounted = false;

    ~ScopedMount();
};

struct ExtractData {
    int flags = 0;
    int exitval = -1;
    std::string fifo_path;
    std::string error_detail;
};

int archive_extract_flags(bool preserve_attributes);
void extract_archive_to_disk(ExtractData* data);
fs::path make_target_extract_path(const fs::path& mountpoint, const std::string& entry_path);
void mount_target_device(const ManifestEntry& entry, const fs::path& mountpoint);

}  // namespace aegis
