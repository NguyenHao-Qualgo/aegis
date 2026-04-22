#include "aegis/installer/handlers.hpp"

#include <fcntl.h>
#include <filesystem>
#include <limits.h>
#include <sys/stat.h>

#include "aegis/common/error.hpp"
#include "aegis/common/logging.hpp"
#include "aegis/core/types.hpp"
#include "aegis/installer/handler_utils.hpp"
#include "aegis/installer/install_context.hpp"
#include "aegis/io/io.hpp"

namespace aegis {

namespace {

int blkprotect(const fs::path& device, bool on) {
    char abs_path[PATH_MAX] = {};
    constexpr char unprot_char = '0';
    constexpr char prot_char = '1';

    if (device.string().rfind("/dev/", 0) != 0) {
        return 0;
    }

    struct stat sb {};
    if (::stat(device.c_str(), &sb) != 0 || !S_ISBLK(sb.st_mode)) {
        return 0;
    }

    if (::realpath(device.c_str(), abs_path) == nullptr) {
        return -errno;
    }

    std::string sysfs_path = "/sys/class/block/";
    sysfs_path += abs_path + 5;
    sysfs_path += "/force_ro";

    if (::access(sysfs_path.c_str(), W_OK) != 0) {
        return 0;
    }

    FileDescriptor fd(::open(sysfs_path.c_str(), O_RDWR));
    if (!fd) {
        return -EBADF;
    }

    char current = 0;
    if (::read(fd.get(), &current, 1) != 1) {
        return -EIO;
    }

    const char requested = on ? prot_char : unprot_char;
    if (requested == current) {
        return 0;
    }
    if (::lseek(fd.get(), 0, SEEK_SET) < 0) {
        return -EIO;
    }
    if (::write(fd.get(), &requested, 1) != 1) {
        return -EIO;
    }
    return 1;
}

class BlockProtectionGuard {
public:
    explicit BlockProtectionGuard(fs::path device) : device_(std::move(device)) {
        status_ = blkprotect(device_, false);
        if (status_ < 0) {
            fail_runtime("failed to disable block protection for " + device_.string());
        }
    }

    ~BlockProtectionGuard() {
        if (status_ == 1) {
            (void)blkprotect(device_, true);
        }
    }

private:
    fs::path device_;
    int status_ = 0;
};

}  // namespace

void RawHandler::install(const InstallContext& ctx,
                         StreamReader &reader,
                         const CpioEntry &cpio_entry,
                         const ManifestEntry &entry,
                         const AesMaterial *aes) {
    ctx.check_cancel();

    if (entry.device.empty()) {
        fail_runtime("raw image missing device for " + entry.filename);
    }

    const fs::path target = entry.device;
    LOG_I("raw handler: target device='" + target.string() + "', mode=direct stream write");

    BlockProtectionGuard protection(target);
    FileDescriptor out(::open(target.c_str(), O_RDWR));
    if (!out) {
        fail_runtime("cannot open device " + target.string());
    }

    PayloadStreamer streamer(ctx);
    stream_payload_to_fd(streamer, reader, cpio_entry, entry, aes, ctx, out.get());

    ctx.check_cancel();
    if (::fsync(out.get()) != 0) {
        fail_runtime("failed to fsync raw device " + target.string());
    }

    LOG_I("raw handler: completed direct stream write to '" + target.string() + "'");
}

}  // namespace aegis
