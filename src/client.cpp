#include "aegis/client.hpp"

#include <iostream>
#include <map>
#include <stdexcept>

#include <sdbus-c++/sdbus-c++.h>

namespace aegis {

int Client::run(const std::vector<std::string>& args) const {
    if (args.empty()) {
        std::cerr << "Usage: aegis <status|install|mark-good|mark-bad|mark-active|get-primary|get-booted>\n";
        return 1;
    }

    auto connection = sdbus::createSystemBusConnection();
    auto proxy = sdbus::createProxy(*connection, "de.skytrack.Aegis", "/de/skytrack/Aegis");
    proxy->finishRegistration();

    const auto& cmd = args[0];
    if (cmd == "status") {
        std::map<std::string, sdbus::Variant> status;
        proxy->callMethod("GetStatus").onInterface("de.skytrack.Aegis1").storeResultsTo(status);
        for (const auto& [key, value] : status) {
            std::cout << key << ": ";
            if (value.containsValueOfType<std::string>()) std::cout << value.get<std::string>();
            else if (value.containsValueOfType<int32_t>()) std::cout << value.get<int32_t>();
            std::cout << '\n';
        }
        return 0;
    }
    if (cmd == "install") {
        if (args.size() < 2) throw std::runtime_error("install requires bundle path");
        proxy->callMethod("Install").onInterface("de.skytrack.Aegis1").withArguments(args[1]);
        return 0;
    }
    if (cmd == "mark-good") {
        proxy->callMethod("MarkGood").onInterface("de.skytrack.Aegis1");
        return 0;
    }
    if (cmd == "mark-bad") {
        proxy->callMethod("MarkBad").onInterface("de.skytrack.Aegis1");
        return 0;
    }
    if (cmd == "mark-active") {
        if (args.size() < 2) throw std::runtime_error("mark-active requires A or B");
        proxy->callMethod("MarkActive").onInterface("de.skytrack.Aegis1").withArguments(args[1]);
        return 0;
    }
    if (cmd == "get-primary") {
        std::string slot;
        proxy->callMethod("GetPrimary").onInterface("de.skytrack.Aegis1").storeResultsTo(slot);
        std::cout << slot << '\n';
        return 0;
    }
    if (cmd == "get-booted") {
        std::string slot;
        proxy->callMethod("GetBooted").onInterface("de.skytrack.Aegis1").storeResultsTo(slot);
        std::cout << slot << '\n';
        return 0;
    }
    throw std::runtime_error("Unknown command: " + cmd);
}

}  // namespace aegis
