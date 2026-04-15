#pragma once

#include <string>
#include <vector>

namespace aegis {

struct CliOptions {
    std::string command;
    std::string config_path = "/etc/aegis/system.conf";
    std::string cert_path;
    std::string key_path;
    std::string keyring_path;
    std::string override_boot_slot;
    std::string mount_prefix;
    std::string mksquashfs_args;
    std::string handler_args;
    std::string output_format = "readable";
    std::string bundle_format = "verity";

    bool detailed = false;
    bool ignore_compat = false;
    bool no_check_time = false;
    bool no_verify = false;

    std::vector<std::string> positional;
    std::vector<std::string> encryption_recipients;
};

enum class ParseAction { Run, ExitSuccess };

struct ParseResult {
    ParseAction action = ParseAction::Run;
    CliOptions options;
};

} // namespace aegis