#include "aegis/utils.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace aegis {

SubprocessResult run_command(const std::vector<std::string>& argv,
                             const std::vector<std::string>& env) {
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0)
        throw AegisError("pipe() failed");

    pid_t pid = fork();
    if (pid < 0) throw AegisError("fork() failed");

    if (pid == 0) {
        // Child
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        std::vector<const char*> c_argv;
        for (auto& a : argv) c_argv.push_back(a.c_str());
        c_argv.push_back(nullptr);

        if (!env.empty()) {
            for (auto& e : env) putenv(const_cast<char*>(e.c_str()));
        }

        execvp(c_argv[0], const_cast<char* const*>(c_argv.data()));
        _exit(127);
    }

    // Parent
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    auto read_fd = [](int fd) -> std::string {
        std::string result;
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            result.append(buf, n);
        close(fd);
        return result;
    };

    SubprocessResult res;
    res.stdout_str = read_fd(stdout_pipe[0]);
    res.stderr_str = read_fd(stderr_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    res.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return res;
}

SubprocessResult run_command_checked(const std::vector<std::string>& argv,
                                     const std::vector<std::string>& env) {
    auto res = run_command(argv, env);
    if (res.exit_code != 0) {
        std::string cmd;
        for (auto& a : argv) { cmd += a + " "; }
        throw AegisError("Command failed (exit " + std::to_string(res.exit_code)
                        + "): " + cmd + "\n" + res.stderr_str);
    }
    return res;
}

bool path_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

uint64_t file_size(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        throw AegisError("Cannot stat: " + path);
    return static_cast<uint64_t>(st.st_size);
}

Result<void> mkdir_p(const std::string& path) {
    std::string cmd = "mkdir -p " + path;
    if (system(cmd.c_str()) != 0)
        return Result<void>::err("Failed to create directory: " + path);
    return Result<void>::ok();
}

Result<void> rm_rf(const std::string& path) {
    std::string cmd = "rm -rf " + path;
    if (system(cmd.c_str()) != 0)
        return Result<void>::err("Failed to remove: " + path);
    return Result<void>::ok();
}

Result<void> copy_file(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in) return Result<void>::err("Cannot open source: " + src);
    std::ofstream out(dst, std::ios::binary);
    if (!out) return Result<void>::err("Cannot open dest: " + dst);
    out << in.rdbuf();
    if (!out) return Result<void>::err("Write failed: " + dst);
    return Result<void>::ok();
}

std::string read_text_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw AegisError("Cannot read file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void write_text_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) throw AegisError("Cannot write file: " + path);
    f << content;
}

std::string random_hex(size_t bytes) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::string result;
    result.reserve(bytes * 2);
    for (size_t i = 0; i < bytes; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", dis(gen));
        result += buf;
    }
    return result;
}

std::string resolve_path(const std::string& base_dir, const std::string& rel_path) {
    if (rel_path.empty()) return {};
    if (rel_path[0] == '/') return rel_path;
    return base_dir + "/" + rel_path;
}

std::string dirname(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return path.substr(0, pos);
}

std::string basename(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

void log(LogLevel level, const char* fmt, ...) {
    const char* prefix = "";
    FILE* out = stderr;
    switch (level) {
        case LogLevel::Debug:   prefix = "[DEBUG] ";   break;
        case LogLevel::Info:    prefix = "[INFO]  ";   break;
        case LogLevel::Warning: prefix = "[WARN]  ";   break;
        case LogLevel::Error:   prefix = "[ERROR] ";   break;
    }
    fprintf(out, "%s", prefix);
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fprintf(out, "\n");
}

} // namespace aegis
