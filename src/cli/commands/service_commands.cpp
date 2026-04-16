#include "aegis/cli/commands.h"

#include "aegis/cli/app_context.h"
#include "aegis/dbus/client.h"
#include "aegis/dbus/service.h"
#include "config.h"

#include <iostream>
#include <string>

namespace aegis {
namespace {

bool require_positional(const CliOptions& opts, std::size_t count, const char* usage) {
    if (opts.positional.size() < count) {
        std::cerr << "Usage: " << usage << "\n";
        return false;
    }
    return true;
}

bool connect_client(AegisDbusClient& client, bool print_error = true) {
    auto result = client.connect_system_bus();
    if (!result) {
        if (print_error) {
            std::cerr << "Error: " << result.error() << "\n";
        }
        return false;
    }
    return true;
}

class StatusPrinter {
  public:
    explicit StatusPrinter(bool detailed) : detailed_(detailed) {}

    void print_summary(const std::string& compatible, const std::string& variant,
                       const std::string& bootloader, const std::string& boot_slot,
                       const std::string& primary) const {
        std::cout << "=== System Info ===\n"
                  << "Compatible:    " << compatible << "\n"
                  << "Variant:       " << variant << "\n"
                  << "Bootloader:    " << bootloader << "\n"
                  << "Boot slot:     " << boot_slot << "\n"
                  << "Primary slot:  " << primary << "\n"
                  << "\n=== Slot Status ===\n";
    }

    void print_slot(const SlotStatusView& slot) const {
        const bool is_booted = get_bool(slot, "booted");
        const bool is_primary = get_bool(slot, "primary");

        std::cout << "\n  Slot " << slot.name;
        if (is_booted) {
            std::cout << " [BOOTED]";
        }
        if (is_primary) {
            std::cout << " [PRIMARY]";
        }
        std::cout << ":\n";

        print_string(slot, "device", "device");
        print_string(slot, "type", "type");
        print_string(slot, "bootname", "bootname");
        print_string(slot, "class", "class");
        print_string(slot, "state", "state");

        if (!detailed_) {
            return;
        }

        print_string(slot, "bundle.compatible", "bundle.compatible");
        print_string(slot, "bundle.version", "bundle.version");
        print_string(slot, "bundle.description", "bundle.description");
        print_string(slot, "bundle.build", "bundle.build");
        print_string(slot, "bundle.hash", "bundle.hash");
        print_string(slot, "sha256", "sha256");
        print_string(slot, "installed.timestamp", "installed.timestamp");
        print_string(slot, "activated.timestamp", "activated.timestamp");
        print_i32(slot, "index", "index");
        print_u64(slot, "size", "size");
        print_u32(slot, "installed.count", "installed.count");
        print_u32(slot, "activated.count", "activated.count");
    }

  private:
    static bool get_bool(const SlotStatusView& slot, const std::string& key) {
        auto it = slot.bool_fields.find(key);
        return it != slot.bool_fields.end() && it->second;
    }

    static void print_string(const SlotStatusView& slot, const std::string& key,
                             const char* label) {
        auto it = slot.string_fields.find(key);
        if (it != slot.string_fields.end()) {
            std::cout << "    " << label << ": " << it->second << "\n";
        }
    }

    static void print_u32(const SlotStatusView& slot, const std::string& key, const char* label) {
        auto it = slot.u32_fields.find(key);
        if (it != slot.u32_fields.end()) {
            std::cout << "    " << label << ": " << it->second << "\n";
        }
    }

    static void print_u64(const SlotStatusView& slot, const std::string& key, const char* label) {
        auto it = slot.u64_fields.find(key);
        if (it != slot.u64_fields.end()) {
            std::cout << "    " << label << ": " << it->second << "\n";
        }
    }

    static void print_i32(const SlotStatusView& slot, const std::string& key, const char* label) {
        auto it = slot.i32_fields.find(key);
        if (it != slot.i32_fields.end()) {
            std::cout << "    " << label << ": " << it->second << "\n";
        }
    }

    bool detailed_;
};

Result<std::string> get_required_property(AegisDbusClient& client, const char* property_name) {
    return client.get_property_string(property_name);
}

} // namespace

int InstallCommand::execute(const CliOptions& opts) {
    if (!require_positional(opts, 1, "aegis install BUNDLEFILE")) {
        return 1;
    }

    AegisDbusClient client;
    if (!connect_client(client)) {
        return 1;
    }

    auto completion = client.install_bundle_with_progress(opts.positional[0], opts.ignore_compat);
    if (!completion) {
        std::cerr << "Error: " << completion.error() << "\n";
        return 1;
    }

    if (completion.value() != 0) {
        auto last_error = client.get_property_string("LastError");
        if (last_error && !last_error.value().empty()) {
            std::cerr << "Error: " << last_error.value() << "\n";
        } else {
            std::cerr << "Error: installation failed.\n";
        }
        return 1;
    }

    std::cout << "Installation successful.\n";
    return 0;
}

int StatusCommand::execute(const CliOptions& opts) {
    AegisDbusClient client;
    if (!connect_client(client)) {
        return 1;
    }

    auto compatible = get_required_property(client, "Compatible");
    auto variant = get_required_property(client, "Variant");
    auto boot_slot = get_required_property(client, "BootSlot");
    auto bootloader = get_required_property(client, "Bootloader");
    auto primary = client.get_primary();
    auto slots = client.get_slot_status();

    if (!compatible) {
        std::cerr << "Error: " << compatible.error() << "\n";
        return 1;
    }
    if (!variant) {
        std::cerr << "Error: " << variant.error() << "\n";
        return 1;
    }
    if (!boot_slot) {
        std::cerr << "Error: " << boot_slot.error() << "\n";
        return 1;
    }
    if (!bootloader) {
        std::cerr << "Error: " << bootloader.error() << "\n";
        return 1;
    }
    if (!primary) {
        std::cerr << "Error: " << primary.error() << "\n";
        return 1;
    }
    if (!slots) {
        std::cerr << "Error: " << slots.error() << "\n";
        return 1;
    }

    StatusPrinter printer(opts.detailed);
    printer.print_summary(compatible.value(), variant.value(), bootloader.value(),
                          boot_slot.value(), primary.value());

    auto ota_state = client.get_property_string("OtaState");
    auto ota_message = client.get_property_string("OtaStatusMessage");
    auto transaction_id = client.get_property_string("TransactionId");
    auto expected_slot = client.get_property_string("ExpectedSlot");
    if (ota_state && ota_message && transaction_id && expected_slot) {
        std::cout << "\n=== OTA State ===\n";
        std::cout << "State:         " << (ota_state ? ota_state.value() : "<unavailable>") << "\n";
        std::cout << "Message:       " << (ota_message ? ota_message.value() : "<unavailable>") << "\n";
        std::cout << "TransactionId: " << (transaction_id ? transaction_id.value() : "<unavailable>") << "\n";
        std::cout << "Expected slot: " << (expected_slot ? expected_slot.value() : "<unavailable>") << "\n";
    }
    for (const auto& slot : slots.value()) {
        printer.print_slot(slot);
    }

    return 0;
}

int MarkCommand::execute(const CliOptions& opts) {
    AegisDbusClient client;
    if (!connect_client(client)) {
        return 1;
    }

    std::string slot_id = opts.positional.empty() ? "" : opts.positional[0];
    std::string state;

    if (opts.command == "mark-good") {
        state = "good";
    } else if (opts.command == "mark-bad") {
        state = "bad";
    } else if (opts.command == "mark-active") {
        if (slot_id.empty()) {
            std::cerr << "Usage: aegis mark-active SLOTNAME\n";
            return 1;
        }
        state = "active";
    } else {
        std::cerr << "Error: unknown mark command\n";
        return 1;
    }

    auto result = client.mark(state, slot_id);
    if (!result) {
        std::cerr << "Error: " << result.error() << "\n";
        return 1;
    }

    std::cout << result.value().message << "\n";
    return 0;
}

int ServiceCommand::execute(const CliOptions& opts) {
    AppContext::init_service(opts);

    auto result = service_run();
    if (!result) {
        std::cerr << "Service error: " << result.error() << "\n";
        return 1;
    }

    return 0;
}

int VersionCommand::execute(const CliOptions& /*opts*/) {
    AegisDbusClient client;
    if (!connect_client(client, false)) {
        std::cout << AEGIS_BUILD_VERSION << "\n";
        return 0;
    }

    auto version = client.get_property_string("ServiceVersion");
    if (!version) {
        std::cout << AEGIS_BUILD_VERSION << "\n";
        return 0;
    }

    std::cout << version.value() << "\n";
    return 0;
}

} // namespace aegis
