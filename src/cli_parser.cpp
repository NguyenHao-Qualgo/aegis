#include "aegis/cli_parser.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace aegis {

void CliParser::print_usage() {
    std::cerr << R"(Usage: aegis [OPTIONS] COMMAND [ARGS]

Commands:
  bundle CONTENTDIR BUNDLEFILE    Create a bundle from content directory
  install BUNDLEFILE              Install a bundle
  info BUNDLEFILE                 Show bundle info
  status                          Show system status
  mark-good [SLOTNAME]            Mark slot as good
  mark-bad [SLOTNAME]             Mark slot as bad
  mark-active SLOTNAME            Set slot as primary boot target
  extract BUNDLEFILE OUTPUTDIR    Extract bundle contents
  resign INBUNDLE OUTBUNDLE       Resign bundle with new keys
  service                         Run as D-Bus service
  mount BUNDLEFILE                Verify and mount a 
  version                         Show service version

Options:
  --conf=PATH                     System config file (default: /etc/aegis/system.conf)
  --cert=PATH                     Signing certificate
  --key=PATH                      Signing key
  --keyring=PATH                  Verification keyring
  --override-boot-slot=BOOTNAME   Override booted slot detection
  --mount=PATH                    Mount prefix
  --bundle-format=FORMAT          Bundle format: plain, verity, crypt (default: verity)
  --mksquashfs-args=ARGS          Extra mksquashfs arguments
  --output-format=FMT             Output format: readable, shell, json
  --detailed                      Show detailed status
  --ignore-compatible             Skip compatible check
  --no-check-time                 Ignore certificate expiry
  --no-verify                     Skip signature verification (info only)
  --recipient=CERTPATH            Encryption recipient (crypt format, repeatable)
  --help                          Show this help
)";
}

ParseResult CliParser::parse(int argc, char* argv[]) const {
    ParseResult result;
    CliOptions& opts = result.options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage();
            result.action = ParseAction::ExitSuccess;
            return result;
        }

        auto try_opt = [&](const std::string& prefix, std::string& target) -> bool {
            if (arg.rfind(prefix, 0) == 0) {
                target = arg.substr(prefix.size());
                return true;
            }
            return false;
        };

        if (try_opt("--conf=", opts.config_path)) continue;
        if (try_opt("--cert=", opts.cert_path)) continue;
        if (try_opt("--key=", opts.key_path)) continue;
        if (try_opt("--keyring=", opts.keyring_path)) continue;
        if (try_opt("--override-boot-slot=", opts.override_boot_slot)) continue;
        if (try_opt("--mount=", opts.mount_prefix)) continue;
        if (try_opt("--bundle-format=", opts.bundle_format)) continue;
        if (try_opt("--mksquashfs-args=", opts.mksquashfs_args)) continue;
        if (try_opt("--output-format=", opts.output_format)) continue;
        if (try_opt("--handler-args=", opts.handler_args)) continue;

        if (arg.rfind("--recipient=", 0) == 0) {
            opts.encryption_recipients.push_back(arg.substr(12));
            continue;
        }

        if (arg == "--detailed") {
            opts.detailed = true;
            continue;
        }
        if (arg == "--ignore-compatible") {
            opts.ignore_compat = true;
            continue;
        }
        if (arg == "--no-check-time") {
            opts.no_check_time = true;
            continue;
        }
        if (arg == "--no-verify") {
            opts.no_verify = true;
            continue;
        }

        if (opts.command.empty()) {
            opts.command = arg;
        } else {
            opts.positional.push_back(arg);
        }
    }

    return result;
}

} // namespace aegis