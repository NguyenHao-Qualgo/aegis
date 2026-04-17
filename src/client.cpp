#include "aegis/client.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <thread>
#include <stdexcept>

#include <sdbus-c++/sdbus-c++.h>
#include <sdbus-c++/Types.h>

namespace aegis {

namespace {

std::map<std::string, sdbus::Variant> fetchStatus(sdbus::IProxy& proxy, const sdbus::InterfaceName& interfaceName) {
    std::map<std::string, sdbus::Variant> status;
    proxy.callMethod("GetStatus").onInterface(interfaceName).storeResultsTo(status);
    return status;
}

std::string getStringField(const std::map<std::string, sdbus::Variant>& status, const std::string& key) {
    const auto it = status.find(key);
    if (it == status.end() || !it->second.containsValueOfType<std::string>()) {
        return {};
    }
    return it->second.get<std::string>();
}

int getIntField(const std::map<std::string, sdbus::Variant>& status, const std::string& key) {
    const auto it = status.find(key);
    if (it == status.end() || !it->second.containsValueOfType<int32_t>()) {
        return 0;
    }
    return it->second.get<int32_t>();
}

void printStatusLine(const std::map<std::string, sdbus::Variant>& status) {
    const auto state = getStringField(status, "State");
    const auto operation = getStringField(status, "Operation");
    const auto progress = getIntField(status, "Progress");
    const auto message = getStringField(status, "Message");

    std::cout << state << " [" << operation << "] " << progress << "%";
    if (!message.empty()) {
        std::cout << " - " << message;
    }
    std::cout << '\n';
}

bool isTerminalState(const std::string& state) {
    return state == "Idle" || state == "Reboot" || state == "Failure";
}

}

int Client::run(const std::vector<std::string>& args) const {
    if (args.empty()) {
        std::cerr << "Usage: aegis <status|install|mark-good|mark-bad|mark-active|get-primary|get-booted>\n";
        return 1;
    }

    const sdbus::BusName serviceName{"de.skytrack.Aegis"};
    const sdbus::InterfaceName interfaceName{"de.skytrack.Aegis1"};
    const sdbus::ObjectPath objectPath{"/de/skytrack/Aegis"};

    auto connection = sdbus::createSystemBusConnection();
    auto proxy = sdbus::createProxy(*connection, serviceName, objectPath);

    const auto& cmd = args[0];
    if (cmd == "status") {
        const auto status = fetchStatus(*proxy, interfaceName);
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

        bool done = false;
        std::string terminalError;

        proxy->uponSignal("StatusChanged")
            .onInterface(interfaceName)
            .call([&](const std::map<std::string, sdbus::Variant>& status) {
                printStatusLine(status);

                const auto state = getStringField(status, "State");
                if (isTerminalState(state)) {
                    if (state == "Failure") {
                        terminalError = getStringField(status, "LastError");
                    }
                    done = true;
                }
            });

        connection->enterEventLoopAsync();

        proxy->callMethodAsync("Install")
            .onInterface(interfaceName)
            .withArguments(args[1]);

        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        connection->leaveEventLoop();

        if (!terminalError.empty()) {
            throw std::runtime_error(terminalError);
        }
        return 0;
    }
    if (cmd == "mark-good") {
        proxy->callMethod("MarkGood").onInterface(interfaceName);
        return 0;
    }
    if (cmd == "mark-bad") {
        proxy->callMethod("MarkBad").onInterface(interfaceName);
        return 0;
    }
    if (cmd == "mark-active") {
        if (args.size() < 2) throw std::runtime_error("mark-active requires A or B");
        proxy->callMethod("MarkActive").onInterface(interfaceName).withArguments(args[1]);
        return 0;
    }
    if (cmd == "get-primary") {
        std::string slot;
        proxy->callMethod("GetPrimary").onInterface(interfaceName).storeResultsTo(slot);
        std::cout << slot << '\n';
        return 0;
    }
    if (cmd == "get-booted") {
        std::string slot;
        proxy->callMethod("GetBooted").onInterface(interfaceName).storeResultsTo(slot);
        std::cout << slot << '\n';
        return 0;
    }
    throw std::runtime_error("Unknown command: " + cmd);
}

}  // namespace aegis
