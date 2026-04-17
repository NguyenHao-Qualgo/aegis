#include "aegis/mount.h"
#include "aegis/utils.h"

#include <sys/mount.h>
#include <sys/stat.h>

namespace aegis {

Result<void> mount(const std::string& source, const std::string& mountpoint,
                   const std::string& fstype, unsigned long flags, const std::string& options) {
    mkdir_p(mountpoint);

    // Use the mount command for better compatibility
    std::vector<std::string> cmd = {"mount"};
    if (!fstype.empty()) {
        cmd.push_back("-t");
        cmd.push_back(fstype);
    }
    if (!options.empty()) {
        cmd.push_back("-o");
        cmd.push_back(options);
    }
    cmd.push_back(source);
    cmd.push_back(mountpoint);

    auto res = run_command(cmd);
    if (res.first != 0)
        return Result<void>::err("mount failed: " + res.second);

    LOG_DEBUG("Mounted %s on %s (type=%s)", source.c_str(), mountpoint.c_str(),
              fstype.empty() ? "auto" : fstype.c_str());
    return Result<void>::ok();
}

Result<void> umount(const std::string& mountpoint) {
    auto res = run_command({"umount", mountpoint});
    if (res.first != 0)
        return Result<void>::err("umount failed for " + mountpoint + ": " + res.second);
    return Result<void>::ok();
}

std::string create_mount_point(const std::string& prefix, const std::string& name) {
    std::string path = prefix + "/" + name;
    mkdir_p(path);
    return path;
}

Result<std::string> mount_squashfs(const std::string& image_path, const std::string& mount_prefix) {
    auto mp = create_mount_point(mount_prefix, "bundle");
    auto res = mount(image_path, mp, "squashfs", MS_RDONLY);
    if (!res)
        return Result<std::string>::err(res.error());
    return Result<std::string>::ok(std::move(mp));
}

Result<void> umount_squashfs(const std::string& mountpoint) {
    return umount(mountpoint);
}

} // namespace aegis
