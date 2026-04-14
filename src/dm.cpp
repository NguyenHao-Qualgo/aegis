#include "aegis/dm.h"
#include "aegis/utils.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/dm-ioctl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace aegis {

namespace {

constexpr uint32_t kDmVersionMajor = DM_VERSION_MAJOR;
constexpr uint32_t kDmVersionMinor = 0;
constexpr uint32_t kDmVersionPatch = 0;
constexpr uint64_t kSectorSize = 512;
constexpr uint64_t kBlockSize = 4096;

#ifndef LOOP_SET_BLOCK_SIZE
#define LOOP_SET_BLOCK_SIZE 0x4C09
#endif

struct DmIoctlBuffer {
    dm_ioctl header;
    dm_target_spec target;
    char params[1024];
};

void set_dm_header(dm_ioctl& hdr,
                   size_t total_size,
                   uint32_t flags,
                   const std::string& name) {
    std::memset(&hdr, 0, sizeof(hdr));
    hdr.version[0] = kDmVersionMajor;
    hdr.version[1] = kDmVersionMinor;
    hdr.version[2] = kDmVersionPatch;
    hdr.data_size = total_size;
    hdr.data_start = sizeof(dm_ioctl);
    hdr.flags = flags;
    if (!name.empty()) {
        std::snprintf(hdr.name, sizeof(hdr.name), "%s", name.c_str());
    }
}

int open_dm_control() {
    int fd = ::open("/dev/mapper/control", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        throw AegisError("Failed to open /dev/mapper/control: " + std::string(std::strerror(errno)));
    }
    return fd;
}

std::string build_verity_params(const std::string& data_device,
                                uint64_t data_size,
                                const std::string& root_hash,
                                const std::string& salt,
                                uint64_t hash_offset) {
    if (data_size == 0 || (data_size % kBlockSize) != 0) {
        throw AegisError("dm-verity data_size must be a non-zero multiple of 4096");
    }
    if ((hash_offset % kBlockSize) != 0) {
        throw AegisError("dm-verity hash_offset must be a multiple of 4096");
    }

    const uint64_t data_blocks = data_size / kBlockSize;
    const uint64_t hash_start_block = hash_offset / kBlockSize;

    return "1 " + data_device + " " + data_device +
           " 4096 4096 " + std::to_string(data_blocks) +
           " " + std::to_string(hash_start_block) +
           " sha256 " + root_hash + " " + salt;
}

std::string build_crypt_params(const std::string& data_device,
                               const std::string& hex_key,
                               const std::string& cipher) {
    return cipher + " " + hex_key + " 0 " + data_device + " 0";
}

void check_ioctl(int rc, const std::string& what) {
    if (rc != 0) {
        throw AegisError(what + ": " + std::string(std::strerror(errno)));
    }
}

void quick_check_device(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw AegisError("Failed to open mapped device " + path + ": " + std::string(std::strerror(errno)));
    }

    char b = 0;
    ssize_t rd = ::read(fd, &b, 1);
    int saved_errno = errno;
    ::close(fd);

    if (rd != 1) {
        throw AegisError("Check read from mapped device failed: " + std::string(std::strerror(saved_errno)));
    }
}

DmTarget create_dm_target(const std::string& dm_name,
                          const std::string& target_type,
                          uint64_t data_size,
                          const std::string& params) {
    if (data_size == 0) {
        throw AegisError("data_size must be > 0");
    }
    if ((data_size % kSectorSize) != 0) {
        throw AegisError("data_size must be a multiple of 512");
    }

    DmIoctlBuffer setup{};
    const size_t total_size = sizeof(setup);

    int dmfd = open_dm_control();
    try {
        set_dm_header(setup.header, total_size, DM_READONLY_FLAG, dm_name);
        check_ioctl(::ioctl(dmfd, DM_DEV_CREATE, &setup), "Failed to create dm device");

        std::memset(&setup, 0, sizeof(setup));
        set_dm_header(setup.header, total_size, DM_READONLY_FLAG, dm_name);
        setup.header.target_count = 1;

        setup.target.status = 0;
        setup.target.sector_start = 0;
        setup.target.length = data_size / kSectorSize;
        std::snprintf(setup.target.target_type, sizeof(setup.target.target_type), "%s", target_type.c_str());

        if (params.size() + 1 > sizeof(setup.params)) {
            throw AegisError("dm parameter string too long");
        }
        std::snprintf(setup.params, sizeof(setup.params), "%s", params.c_str());

        size_t next = sizeof(dm_target_spec) + std::strlen(setup.params) + 1;
        next = (next + 7u) & ~7u;
        setup.target.next = static_cast<uint32_t>(next);

        check_ioctl(::ioctl(dmfd, DM_TABLE_LOAD, &setup), "Failed to load dm table");

        std::memset(&setup, 0, sizeof(setup));
        set_dm_header(setup.header, total_size, 0, dm_name);
        check_ioctl(::ioctl(dmfd, DM_DEV_SUSPEND, &setup), "Failed to resume dm device");

        DmTarget target;
        target.dm_name = dm_name;
        target.dm_device = "/dev/dm-" + std::to_string(minor(setup.header.dev));
        target.active = true;

        quick_check_device(target.dm_device);

        LOG_INFO("dm target created: %s (%s)", target.dm_device.c_str(), target_type.c_str());
        ::close(dmfd);
        return target;

    } catch (...) {
        std::memset(&setup, 0, sizeof(setup));
        set_dm_header(setup.header, total_size, 0, dm_name);
        ::ioctl(dmfd, DM_DEV_REMOVE, &setup);
        ::close(dmfd);
        throw;
    }
}

} // namespace

LoopDevice loop_setup(const std::string& file_path, uint64_t offset, uint64_t size) {
    int backing_fd = -1;
    int control_fd = -1;
    int loop_fd = -1;

    try {
        backing_fd = ::open(file_path.c_str(), O_RDONLY | O_CLOEXEC);
        if (backing_fd < 0) {
            throw AegisError("Failed to open bundle file " + file_path + ": " + std::string(std::strerror(errno)));
        }

        control_fd = ::open("/dev/loop-control", O_RDWR | O_CLOEXEC);
        if (control_fd < 0) {
            throw AegisError("Failed to open /dev/loop-control: " + std::string(std::strerror(errno)));
        }

        int loop_idx = -1;
        std::string loop_path;

        for (int tries = 0; tries < 10; ++tries) {
            loop_idx = ::ioctl(control_fd, LOOP_CTL_GET_FREE);
            if (loop_idx < 0) {
                throw AegisError("Failed to get free loop device: " + std::string(std::strerror(errno)));
            }

            loop_path = "/dev/loop" + std::to_string(loop_idx);
            loop_fd = ::open(loop_path.c_str(), O_RDONLY | O_CLOEXEC);
            if (loop_fd < 0) {
                if (errno == ENOENT || errno == ENXIO) {
                    continue;
                }
                throw AegisError("Failed to open " + loop_path + ": " + std::string(std::strerror(errno)));
            }

            if (::ioctl(loop_fd, LOOP_SET_FD, backing_fd) == 0) {
                ::close(backing_fd);
                backing_fd = -1;
                ::close(control_fd);
                control_fd = -1;

                loop_info64 info{};
                info.lo_offset = offset;
                info.lo_sizelimit = size;
                info.lo_flags = LO_FLAGS_READ_ONLY | LO_FLAGS_AUTOCLEAR;

                if (::ioctl(loop_fd, LOOP_SET_STATUS64, &info) != 0) {
                    int saved_errno = errno;
                    ::ioctl(loop_fd, LOOP_CLR_FD, 0);
                    ::close(loop_fd);
                    throw AegisError("Failed to configure loop device: " + std::string(std::strerror(saved_errno)));
                }

                if (::ioctl(loop_fd, LOOP_SET_BLOCK_SIZE, kBlockSize) != 0) {
                    LOG_WARNING("Failed to set loop block size to 4096: %s", std::strerror(errno));
                }

                LoopDevice loop;
                loop.path = loop_path;
                loop.fd = loop_fd;

                LOG_INFO("Loop device setup: %s -> %s (offset=%lu size=%lu)",
                         file_path.c_str(), loop.path.c_str(), offset, size);
                return loop;
            }

            int saved_errno = errno;
            ::close(loop_fd);
            loop_fd = -1;

            if (saved_errno == EBUSY) {
                continue;
            }

            throw AegisError("Failed to bind file to loop device: " + std::string(std::strerror(saved_errno)));
        }

        throw AegisError("Failed to find a usable free loop device");

    } catch (...) {
        if (loop_fd >= 0) {
            ::close(loop_fd);
        }
        if (control_fd >= 0) {
            ::close(control_fd);
        }
        if (backing_fd >= 0) {
            ::close(backing_fd);
        }
        throw;
    }
}

Result<void> loop_teardown(const LoopDevice& loop) {
    if (loop.path.empty()) {
        return Result<void>::ok();
    }

    int fd = loop.fd;
    if (fd < 0) {
        fd = ::open(loop.path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return Result<void>::err(
                "Failed to open loop device " + loop.path + ": " + std::string(std::strerror(errno)));
        }
    }

    if (::ioctl(fd, LOOP_CLR_FD, 0) != 0) {
        int saved_errno = errno;
        if (loop.fd < 0) {
            ::close(fd);
        } else {
            ::close(loop.fd);
        }
        return Result<void>::err(
            "Failed to detach loop device " + loop.path + ": " + std::string(std::strerror(saved_errno)));
    }

    if (loop.fd < 0) {
        ::close(fd);
    } else {
        ::close(loop.fd);
    }

    LOG_DEBUG("Loop device removed: %s", loop.path.c_str());
    return Result<void>::ok();
}

DmTarget dm_verity_setup(const std::string& data_device,
                         uint64_t data_size,
                         const std::string& root_hash,
                         const std::string& salt,
                         uint64_t hash_offset) {
    std::string dm_name = "aegis-verity-" + random_hex(4);
    std::string params = build_verity_params(data_device, data_size, root_hash, salt, hash_offset);
    return create_dm_target(dm_name, "verity", data_size, params);
}

DmTarget dm_crypt_setup(const std::string& data_device,
                        uint64_t data_size,
                        const std::string& hex_key,
                        const std::string& cipher) {
    std::string dm_name = "aegis-crypt-" + random_hex(4);
    std::string params = build_crypt_params(data_device, hex_key, cipher);
    return create_dm_target(dm_name, "crypt", data_size, params);
}

Result<void> dm_remove(const std::string& dm_name) {
    if (dm_name.empty()) {
        return Result<void>::ok();
    }

    struct {
        dm_ioctl header;
    } setup{};

    int dmfd = ::open("/dev/mapper/control", O_RDWR | O_CLOEXEC);
    if (dmfd < 0) {
        return Result<void>::err(
            "Failed to open /dev/mapper/control: " + std::string(std::strerror(errno)));
    }

    set_dm_header(setup.header, sizeof(setup), 0, dm_name);

    if (::ioctl(dmfd, DM_DEV_REMOVE, &setup) != 0) {
        int saved_errno = errno;
        ::close(dmfd);
        return Result<void>::err(
            "Failed to remove dm device " + dm_name + ": " + std::string(std::strerror(saved_errno)));
    }

    ::close(dmfd);
    LOG_DEBUG("Removed dm target: %s", dm_name.c_str());
    return Result<void>::ok();
}

} // namespace aegis
