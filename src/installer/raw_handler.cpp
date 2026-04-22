#include "aegis/installer/handlers.hpp"

#include <fcntl.h>
#include <filesystem>
#include <limits.h>
#include <sys/stat.h>

#include "aegis/io/io.hpp"
#include "aegis/common/logging.hpp"
#include "aegis/crypto/payload.hpp"
#include "aegis/core/types.hpp"

namespace aegis {

namespace {

int blkprotect(const fs::path &device, bool on) {
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

void install_raw_image(StreamReader &reader,
                       const CpioEntry &cpio_entry,
                       const ManifestEntry &entry,
                       const AesMaterial *aes) {
    if (entry.device.empty()) {
        fail_runtime("raw image missing device for " + entry.filename);
    }

    const fs::path target = entry.device;
    LOG_I("raw handler: target device='" + target.string() +
               "', mode=direct stream write");
    int prot_stat = blkprotect(target, false);
    if (prot_stat < 0) {
        fail_runtime("failed to disable block protection for " + target.string());
    }
    FileDescriptor out;
    out.reset(::open(target.c_str(), O_RDWR));
    if (!out) {
        fail_runtime("cannot open device " + target.string());
    }

    auto sink = [&](const char *data, std::size_t len) {
        write_all_fd(out.get(), data, len);
    };

    if (entry.encrypted) {
        if (!aes) {
            fail_runtime("encrypted payload requires --aes-key");
        }
        stream_encrypted_payload(reader, cpio_entry, *aes, entry.ivt, sink, entry.sha256);
    } else {
        stream_plain_payload(reader, cpio_entry, sink, entry.sha256);
    }

    if (prot_stat == 1) {
        ::fsync(out.get());
        (void)blkprotect(target, true);
    }

    LOG_I("raw handler: completed direct stream write to '" + target.string() + "'");
}

}  // namespace

void RawHandler::install(StreamReader &reader,
                         const CpioEntry &cpio_entry,
                         const ManifestEntry &entry,
                         const AesMaterial *aes) {
    install_raw_image(reader, cpio_entry, entry, aes);
}

}  // namespace aegis
