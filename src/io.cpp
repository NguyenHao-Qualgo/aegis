#include "aegis/io.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

namespace aegis {

void write_all_fd(int fd, const char *data, std::size_t len) {
    while (len > 0) {
        const ssize_t rc = ::write(fd, data, len);
        if (rc < 0) {
            if (errno == EPIPE) {
                fail_runtime("write failed: downstream reader closed the stream (EPIPE)");
            }
            fail_runtime(std::string("write failed: ") + std::strerror(errno));
        }
        data += rc;
        len -= static_cast<std::size_t>(rc);
    }
}

void ensure_parent_dir(const fs::path &path) {
    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) { fail_runtime("cannot create directory: " + parent.string()); }
    }
}

int wait_child(pid_t pid, const std::string &name) {
    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) { fail_runtime("waitpid failed for " + name); }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { fail_runtime(name + " failed"); }
    return 0;
}

void reap_child(pid_t pid) {
    if (pid < 0) { return; }
    int status = 0;
    (void)::waitpid(pid, &status, 0);
}

ChildProcess spawn_process(const std::vector<std::string> &args,
                           bool connect_stdin,
                           bool connect_stdout,
                           const std::string &chdir_to) {
    int stdin_pipe[2]  = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    if (connect_stdin && ::pipe(stdin_pipe) != 0) { fail_runtime("pipe failed"); }
    if (connect_stdout && ::pipe(stdout_pipe) != 0) {
        if (stdin_pipe[0] >= 0) { ::close(stdin_pipe[0]); ::close(stdin_pipe[1]); }
        fail_runtime("pipe failed");
    }

    pid_t pid = ::fork();
    if (pid < 0) { fail_runtime("fork failed"); }
    if (pid == 0) {
        if (!chdir_to.empty() && ::chdir(chdir_to.c_str()) != 0) { _exit(127); }
        if (connect_stdin)  { ::dup2(stdin_pipe[0],  STDIN_FILENO); }
        if (connect_stdout) { ::dup2(stdout_pipe[1], STDOUT_FILENO); }
        if (connect_stdin)  { ::close(stdin_pipe[0]);  ::close(stdin_pipe[1]); }
        if (connect_stdout) { ::close(stdout_pipe[0]); ::close(stdout_pipe[1]); }
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (const auto &arg : args) { argv.push_back(const_cast<char *>(arg.c_str())); }
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        _exit(127);
    }

    ChildProcess proc;
    proc.pid = pid;
    if (connect_stdin)  { ::close(stdin_pipe[0]);  proc.stdin_fd.reset(stdin_pipe[1]); }
    if (connect_stdout) { ::close(stdout_pipe[1]); proc.stdout_fd.reset(stdout_pipe[0]); }
    return proc;
}

}  // namespace aegis
