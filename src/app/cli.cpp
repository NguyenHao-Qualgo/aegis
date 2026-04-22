#include "aegis/app/cli.hpp"
#include "aegis/common/util.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <sdbus-c++/Types.h>
#include <sdbus-c++/sdbus-c++.h>

namespace aegis {

namespace {

volatile sig_atomic_t gInterrupted = 0;

void signalHandler(int /*signum*/) {
    std::cerr << "\nCtrl+C pressed. Exiting aegis installation client...\n";
    std::cerr << "Note that this will not abort the installation running in the aegis service!\n";
    gInterrupted = 1;
}

struct DbusContext {
    static constexpr const char* kServiceName = "de.skytrack.Aegis";
    static constexpr const char* kInterfaceName = "de.skytrack.Aegis1";
    static constexpr const char* kObjectPath = "/de/skytrack/Aegis";
};

void printUsage(std::ostream& os) {
    os
        << "Usage:\n"
        << "  aegis <command> [arguments]\n"
        << "\n"
        << "Commands:\n"
        << "  status                 Show current daemon status\n"
        << "  install <bundle>       Request installation of a bundle/image\n"
        << "  mark-active <A|B>      Mark slot A or B as active\n"
        << "  get-primary            Show primary slot\n"
        << "  get-booted             Show currently booted slot\n"
        << "  help [command]         Show general or command-specific help\n"
        << "\n"
        << "Examples:\n"
        << "  aegis status\n"
        << "  aegis install /data/update.swu\n"
        << "  aegis mark-active B\n"
        << "  aegis get-primary\n"
        << "  aegis get-booted\n";
}

void printCommandHelp(const std::string& command, std::ostream& os) {
    if (command == "status") {
        os
            << "Usage:\n"
            << "  aegis status\n"
            << "\n"
            << "Description:\n"
            << "  Query the aegis daemon and print the current status fields.\n";
        return;
    }

    if (command == "install") {
        os
            << "Usage:\n"
            << "  aegis install <bundle-path>\n"
            << "\n"
            << "Description:\n"
            << "  Request installation of a bundle/image through the aegis daemon.\n"
            << "  The client prints progress updates from DBus until a terminal state\n"
            << "  is reached.\n"
            << "\n"
            << "Example:\n"
            << "  aegis install /data/update.swu\n";
        return;
    }

    if (command == "mark-active") {
        os
            << "Usage:\n"
            << "  aegis mark-active <A|B>\n"
            << "\n"
            << "Description:\n"
            << "  Mark the specified slot as active.\n"
            << "\n"
            << "Example:\n"
            << "  aegis mark-active A\n";
        return;
    }

    if (command == "get-primary") {
        os
            << "Usage:\n"
            << "  aegis get-primary\n"
            << "\n"
            << "Description:\n"
            << "  Print the primary slot reported by the daemon.\n";
        return;
    }

    if (command == "get-booted") {
        os
            << "Usage:\n"
            << "  aegis get-booted\n"
            << "\n"
            << "Description:\n"
            << "  Print the currently booted slot reported by the daemon.\n";
        return;
    }

    os << "Unknown command: " << command << '\n';
    printUsage(os);
}

std::map<std::string, sdbus::Variant> fetchStatus(
    sdbus::IProxy& proxy,
    const sdbus::InterfaceName& interfaceName) {
    std::map<std::string, sdbus::Variant> status;
    proxy.callMethod("GetStatus").onInterface(interfaceName).storeResultsTo(status);
    return status;
}

std::string getStringField(const std::map<std::string, sdbus::Variant>& status,
                           const std::string& key) {
    const auto it = status.find(key);
    if (it == status.end() || !it->second.containsValueOfType<std::string>()) {
        return {};
    }
    return it->second.get<std::string>();
}

int getIntField(const std::map<std::string, sdbus::Variant>& status,
                const std::string& key) {
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

int handleStatus(sdbus::IProxy& proxy, const sdbus::InterfaceName& interfaceName) {
    const auto status = fetchStatus(proxy, interfaceName);
    for (const auto& [key, value] : status) {
        std::cout << key << ": ";
        if (value.containsValueOfType<std::string>()) {
            std::cout << value.get<std::string>();
        } else if (value.containsValueOfType<int32_t>()) {
            std::cout << value.get<int32_t>();
        } else {
            std::cout << "<unsupported type>";
        }
        std::cout << '\n';
    }
    return 0;
}

int handleInstall(sdbus::IProxy& proxy,
                  sdbus::IConnection& connection,
                  const sdbus::InterfaceName& interfaceName,
                  const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw std::runtime_error("install requires <bundle-path>\nTry: aegis help install");
    }

    bool done = false;
    std::string terminalError;

    proxy.uponSignal("StatusChanged")
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

    connection.enterEventLoopAsync();

    proxy.callMethod("Install")
        .onInterface(interfaceName)
        .withArguments(args[1]);

    while (!done && !gInterrupted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    connection.leaveEventLoop();

    if (!terminalError.empty()) {
        throw std::runtime_error(terminalError);
    }

    return 0;
}

int handleMarkActive(sdbus::IProxy& proxy,
                     const sdbus::InterfaceName& interfaceName,
                     const std::vector<std::string>& args) {
    if (args.size() < 2) {
        throw std::runtime_error("mark-active requires <A|B>\nTry: aegis help mark-active");
    }

    const auto& slot = args[1];
    if (slot != "A" && slot != "B") {
        throw std::runtime_error("mark-active only accepts A or B");
    }

    proxy.callMethod("MarkActive")
        .onInterface(interfaceName)
        .withArguments(slot);
    return 0;
}

int handleGetPrimary(sdbus::IProxy& proxy, const sdbus::InterfaceName& interfaceName) {
    std::string slot;
    proxy.callMethod("GetPrimary").onInterface(interfaceName).storeResultsTo(slot);
    std::cout << slot << '\n';
    return 0;
}

int handleGetBooted(sdbus::IProxy& proxy, const sdbus::InterfaceName& interfaceName) {
    std::string slot;
    proxy.callMethod("GetBooted").onInterface(interfaceName).storeResultsTo(slot);
    std::cout << slot << '\n';
    return 0;
}

}  // namespace

int Cli::run(const std::vector<std::string>& args) const {
    if (args.empty()) {
        printUsage(std::cerr);
        return 1;
    }

    const auto& cmd = args[0];
    if (cmd == "-h" || cmd == "--help") {
        printUsage(std::cout);
        return 0;
    }
    if (cmd == "help") {
        if (args.size() >= 2) {
            printCommandHelp(args[1], std::cout);
        } else {
            printUsage(std::cout);
        }
        return 0;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    const sdbus::BusName serviceName{DbusContext::kServiceName};
    const sdbus::InterfaceName interfaceName{DbusContext::kInterfaceName};
    const sdbus::ObjectPath objectPath{DbusContext::kObjectPath};

    auto connection = sdbus::createSystemBusConnection();
    auto proxy = sdbus::createProxy(*connection, serviceName, objectPath);

    if (cmd == "status") {
        return handleStatus(*proxy, interfaceName);
    }
    if (cmd == "install") {
        return handleInstall(*proxy, *connection, interfaceName, args);
    }
    if (cmd == "mark-active") {
        return handleMarkActive(*proxy, interfaceName, args);
    }
    if (cmd == "get-primary") {
        return handleGetPrimary(*proxy, interfaceName);
    }
    if (cmd == "get-booted") {
        return handleGetBooted(*proxy, interfaceName);
    }

    std::cerr << "Unknown command: " << cmd << "\n\n";
    printUsage(std::cerr);
    return 1;
}

}  // namespace aegis