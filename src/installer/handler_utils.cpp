#include "aegis/installer/handler_utils.hpp"

#include <cerrno>
#include <cstring>

namespace aegis {

void write_all_checked(int fd,
                       const char* data,
                       std::size_t len,
                       const InstallContext& ctx,
                       const std::string& broken_pipe_message) {
    while (len > 0) {
        ctx.check_cancel();

        const ssize_t rc = ::write(fd, data, len);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EPIPE && !broken_pipe_message.empty()) {
                fail_runtime(broken_pipe_message);
            }
            fail_runtime(std::string("write failed: ") + std::strerror(errno));
        }

        data += rc;
        len -= static_cast<std::size_t>(rc);
    }
}

}  // namespace aegis
