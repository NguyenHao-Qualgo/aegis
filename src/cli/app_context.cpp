#include "aegis/cli/app_context.h"
#include "aegis/context.h"

namespace aegis {

void AppContext::init_runtime(const CliOptions& opts) {
    Context::instance().init(opts.config_path, {}, {}, opts.keyring_path, opts.override_boot_slot,
                             opts.mount_prefix);
}

void AppContext::init_service(const CliOptions& opts) {
    Context::instance().init(opts.config_path, opts.cert_path, opts.key_path, opts.keyring_path,
                             opts.override_boot_slot, opts.mount_prefix);
}

} // namespace aegis
