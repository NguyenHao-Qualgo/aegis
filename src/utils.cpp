#include "aegis/utils.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <spawn.h>
#include <sstream>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

extern char** environ;

namespace aegis {
namespace {

std::string shell_escape(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

std::string build_shell_command(const std::vector<std::string>& argv,
                                const std::vector<std::string>& env) {
    if (argv.empty()) {
        throw std::runtime_error("argv must not be empty");
    }

    std::ostringstream oss;

    for (const auto& e : env) {
        auto pos = e.find('=');
        if (pos == std::string::npos || pos == 0) {
            throw std::runtime_error("Invalid environment entry: " + e);
        }

        std::string key = e.substr(0, pos);
        std::string value = e.substr(pos + 1);
        oss << key << "=" << shell_escape(value) << " ";
    }

    for (size_t i = 0; i < argv.size(); ++i) {
        if (i != 0) {
            oss << " ";
        }
        oss << shell_escape(argv[i]);
    }

    oss << " 2>&1";
    return oss.str();
}

} // namespace

std::pair<int, std::string> run_command(const std::vector<std::string>& argv,
                                        const std::vector<std::string>& env) {
    std::string cmd = build_shell_command(argv, env);

    std::array<char, 128> buffer;
    std::string output;
    int return_code = -1;

    auto pclose_wrapper = [&return_code](FILE* file) {
        if (file != nullptr) {
            return_code = pclose(file);
        }
    };

    {
        const auto pipe =
            std::unique_ptr<FILE, decltype(pclose_wrapper)>(popen(cmd.c_str(), "r"), pclose_wrapper);

        if (!pipe) {
            throw std::runtime_error("popen() failed");
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
            output += buffer.data();
        }
    }

    if (return_code != -1 && WIFEXITED(return_code)) {
        return_code = WEXITSTATUS(return_code);
    } else if (return_code != -1 && WIFSIGNALED(return_code)) {
        return_code = 128 + WTERMSIG(return_code);
    }

    return {return_code, output};
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
        return Result<void>::err("Failed to copy file from " + src + " to " + dst + ": " +
                                 ec.message());
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
    case LogLevel::Debug:
        prefix = "[DEBUG] ";
        break;
    case LogLevel::Info:
        prefix = "[INFO]  ";
        break;
    case LogLevel::Warning:
        prefix = "[WARN]  ";
        break;
    case LogLevel::Error:
        prefix = "[ERROR] ";
        break;
    }

    std::fprintf(out, "%s", prefix);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(out, fmt, args);
    va_end(args);

    std::fprintf(out, "\n");
}

} // namespace aegis
