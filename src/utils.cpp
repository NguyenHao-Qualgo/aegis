#include "aegis/utils.h"

#include <boost/process.hpp>
#include <boost/process/environment.hpp>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace bp = boost::process;
namespace fs = std::filesystem;

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

bp::environment make_environment(const std::vector<std::string>& env_overrides) {
    bp::environment env = boost::this_process::environment();

    for (const auto& item : env_overrides) {
        auto pos = item.find('=');
        if (pos == std::string::npos || pos == 0) {
            throw AegisError("Invalid environment entry: " + item);
        }

        const std::string key = item.substr(0, pos);
        const std::string value = item.substr(pos + 1);
        env[key] = value;
    }

    return env;
}

void read_stream_into(bp::ipstream& stream, std::string& out) {
    std::string line;
    while (std::getline(stream, line)) {
        out += line;
        out += '\n';
    }
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

    std::vector<std::string> args;
    if (argv.size() > 1) {
        args.assign(argv.begin() + 1, argv.end());
    }

    bp::ipstream stdout_stream;
    bp::ipstream stderr_stream;
    std::error_code ec;

    bp::child child_proc(
        argv[0],
        bp::args(args),
        make_environment(env),
        bp::std_out > stdout_stream,
        bp::std_err > stderr_stream,
        ec
    );

    if (ec) {
        throw AegisError("Failed to start command: " + join_command(argv) +
                         ": " + ec.message());
    }

    SubprocessResult result;
    read_stream_into(stdout_stream, result.stdout_str);
    read_stream_into(stderr_stream, result.stderr_str);

    child_proc.wait(ec);
    if (ec) {
        throw AegisError("Failed to wait for command: " + join_command(argv) +
                         ": " + ec.message());
    }

    result.exit_code = child_proc.exit_code();
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