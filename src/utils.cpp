#include "aegis/utils.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <spawn.h>
#include <random>
#include <sstream>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

extern char **environ;

namespace aegis {
namespace {

std::string join_command(const std::vector<std::string>& argv) {
    std::ostringstream oss;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i != 0) {
            oss << ' ';
        }
        oss << argv[i];
    }
    return oss.str();
}

bool contains_slash(const std::string& s) {
    return s.find('/') != std::string::npos;
}

std::vector<std::string> build_environment(const std::vector<std::string>& env_overrides) {
    std::vector<std::string> envp;
    for (char** current = environ; current && *current; ++current) {
        envp.emplace_back(*current);
    }

    for (const auto& item : env_overrides) {
        auto pos = item.find('=');
        if (pos == std::string::npos || pos == 0) {
            throw AegisError("Invalid environment entry: " + item);
        }

        const std::string key = item.substr(0, pos);
        bool replaced = false;
        for (auto& existing : envp) {
            if (existing.rfind(key + "=", 0) == 0) {
                existing = item;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            envp.push_back(item);
        }
    }

    return envp;
}

std::string read_fd_into_string(int fd) {
    std::string out;
    char buffer[4096];
    while (true) {
        ssize_t n = ::read(fd, buffer, sizeof(buffer));
        if (n == 0)
            break;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        out.append(buffer, static_cast<size_t>(n));
    }
    ::close(fd);
    return out;
}

} // namespace

SubprocessResult run_command(const std::vector<std::string>& argv,
                             const std::vector<std::string>& env) {
    if (argv.empty()) {
        throw AegisError("run_command(): argv must not be empty");
    }

    if (argv[0].empty()) {
        throw AegisError("run_command(): argv[0] must not be empty");
    }

    if (contains_slash(argv[0]) && !path_exists(argv[0])) {
        throw AegisError("Command does not exist: " + argv[0]);
    }

    int stdout_pipe[2];
    int stderr_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) {
        throw AegisError("Failed to create pipes for command: " + join_command(argv) +
                         ": " + std::string(std::strerror(errno)));
    }

    std::vector<std::string> env_strings = build_environment(env);
    std::vector<char*> envp;
    envp.reserve(env_strings.size() + 1);
    for (auto& item : env_strings) {
        envp.push_back(item.data());
    }
    envp.push_back(nullptr);

    std::vector<char*> exec_argv;
    exec_argv.reserve(argv.size() + 1);
    for (const auto& arg : argv) {
        exec_argv.push_back(const_cast<char*>(arg.c_str()));
    }
    exec_argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]);
        ::close(stderr_pipe[1]);
        throw AegisError("Failed to start command: " + join_command(argv) +
                         ": " + std::string(std::strerror(errno)));
    }

    if (pid == 0) {
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]);
        ::close(stderr_pipe[1]);

        if (contains_slash(argv[0])) {
            ::execve(argv[0].c_str(), exec_argv.data(), envp.data());
        } else {
            ::execvpe(argv[0].c_str(), exec_argv.data(), envp.data());
        }
        _exit(errno == ENOENT ? 127 : 126);
    }

    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    SubprocessResult result;
    result.stdout_str = read_fd_into_string(stdout_pipe[0]);
    result.stderr_str = read_fd_into_string(stderr_pipe[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            throw AegisError("Failed to wait for command: " + join_command(argv) +
                             ": " + std::string(std::strerror(errno)));
        }
    }

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    } else {
        result.exit_code = -1;
    }
    return result;
}

SubprocessResult run_command_checked(const std::vector<std::string>& argv,
                                     const std::vector<std::string>& env) {
    auto res = run_command(argv, env);
    if (res.exit_code != 0) {
        throw AegisError(
            "Command failed (exit " + std::to_string(res.exit_code) + "): " +
            join_command(argv) +
            (res.stderr_str.empty() ? "" : "\n" + res.stderr_str));
    }
    return res;
}

bool path_exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(fs::path(path), ec);
}

uint64_t file_size(const std::string& path) {
    std::error_code ec;
    const auto size = fs::file_size(fs::path(path), ec);
    if (ec) {
        throw AegisError("Cannot stat: " + path + ": " + ec.message());
    }
    return static_cast<uint64_t>(size);
}

Result<void> mkdir_p(const std::string& path) {
    std::error_code ec;
    if (path.empty()) {
        return Result<void>::err("Failed to create directory: empty path");
    }

    fs::create_directories(fs::path(path), ec);
    if (ec) {
        return Result<void>::err("Failed to create directory: " + path + ": " + ec.message());
    }

    return Result<void>::ok();
}

Result<void> rm_rf(const std::string& path) {
    std::error_code ec;
    if (path.empty()) {
        return Result<void>::err("Failed to remove: empty path");
    }

    fs::remove_all(fs::path(path), ec);
    if (ec) {
        return Result<void>::err("Failed to remove: " + path + ": " + ec.message());
    }

    return Result<void>::ok();
}

Result<void> copy_file(const std::string& src, const std::string& dst) {
    std::error_code ec;
    fs::copy_file(fs::path(src), fs::path(dst), fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return Result<void>::err("Failed to copy file from " + src + " to " + dst +
                                 ": " + ec.message());
    }
    return Result<void>::ok();
}

std::string read_text_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        throw AegisError("Cannot read file: " + path);
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void write_text_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) {
        throw AegisError("Cannot write file: " + path);
    }

    f << content;
    if (!f) {
        throw AegisError("Write failed: " + path);
    }
}

std::string random_hex(size_t bytes) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string result;
    result.reserve(bytes * 2);

    for (size_t i = 0; i < bytes; ++i) {
        char buf[3];
        std::snprintf(buf, sizeof(buf), "%02x", dis(gen));
        result += buf;
    }

    return result;
}

std::string resolve_path(const std::string& base_dir, const std::string& rel_path) {
    if (rel_path.empty()) {
        return {};
    }
    if (rel_path[0] == '/') {
        return rel_path;
    }
    return (fs::path(base_dir) / rel_path).string();
}

std::string dirname(const std::string& path) {
    const fs::path p(path);
    if (p.parent_path().empty()) {
        return ".";
    }
    return p.parent_path().string();
}

std::string basename(const std::string& path) {
    return fs::path(path).filename().string();
}

void log(LogLevel level, const char* fmt, ...) {
    const char* prefix = "";
    FILE* out = stderr;

    switch (level) {
        case LogLevel::Debug:   prefix = "[DEBUG] "; break;
        case LogLevel::Info:    prefix = "[INFO]  "; break;
        case LogLevel::Warning: prefix = "[WARN]  "; break;
        case LogLevel::Error:   prefix = "[ERROR] "; break;
    }

    std::fprintf(out, "%s", prefix);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(out, fmt, args);
    va_end(args);

    std::fprintf(out, "\n");
}

} // namespace aegis
