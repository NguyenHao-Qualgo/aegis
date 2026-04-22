#include "aegis/common/util.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "aegis/io/command_runner.hpp"
namespace aegis {

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::vector<std::string> split(const std::string& value, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, delim)) {
        parts.push_back(item);
    }
    return parts;
}

std::string joinPath(const std::string& lhs, const std::string& rhs) {
    return (std::filesystem::path(lhs) / rhs).string();
}

std::string currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream os;
    os << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

bool fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

std::string readFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream os;
    os << ifs.rdbuf();
    return os.str();
}

void writeFile(const std::string& path, const std::string& content) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream ofs(path);
    if (!ofs) {
        throw std::runtime_error("Cannot write file: " + path);
    }
    ofs << content;
}

std::string shellQuote(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

std::string getOptionValue(const std::vector<std::string>& args, const std::string& option) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == option) {
            return args[i + 1];
        }
    }
    return {};
}

bool hasOption(const std::vector<std::string>& args, const std::string& option) {
    for (const auto& arg : args) {
        if (arg == option) {
            return true;
        }
    }
    return false;
}

std::string detectFilesystemType(const std::string& device) {
    CommandRunner runner;
    const auto result =
        runner.run("lsblk -no FSTYPE " + shellQuote(device) + " 2>/dev/null");

    if (result.exitCode != 0) {
        return {};
    }

    return trim(result.output);
}

bool hasFilesystemType(const std::string& device, const std::string& expectedType) {
    return detectFilesystemType(device) == expectedType;
}

void makeExt4Filesystem(const std::string& device, bool force) {
    CommandRunner runner;

    std::string command = "mkfs.ext4 ";
    if (force) {
        command += "-F ";
    }
    command += shellQuote(device);

    runner.runOrThrow(command);
}

}  // namespace aegis
