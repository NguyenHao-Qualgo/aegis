#pragma once

#include "rauc/error.h"

#include <string>

namespace rauc {

/// Mount a filesystem
Result<void> mount(const std::string& source,
                   const std::string& mountpoint,
                   const std::string& fstype = {},
                   unsigned long flags = 0,
                   const std::string& options = {});

/// Unmount a filesystem
Result<void> umount(const std::string& mountpoint);

/// Create a temporary mount point under the configured prefix
std::string create_mount_point(const std::string& prefix, const std::string& name);

/// Mount a squashfs image (via loop device or dm)
Result<std::string> mount_squashfs(const std::string& image_path,
                                   const std::string& mount_prefix);

/// Unmount and clean up a squashfs mount
Result<void> umount_squashfs(const std::string& mountpoint);

} // namespace rauc
