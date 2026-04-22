#pragma once
#include <client/linux/handler/exception_handler.h>
#include <cxxabi.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "env.h"
#include "logging.h"
#include "utils.h"

#define xstr(s) _str(s)
#define _str(s) #s

class ServiceBase {
   public:
    ServiceBase() {
        running.store(true);
        instance = this;
        std::string dump_path = Env::ConfigBaseDirectory / "dumps";
        createFolderIfNotExist(dump_path);
        google_breakpad::MinidumpDescriptor descriptor(dump_path);
        exception_handler_ =
            std::make_unique<google_breakpad::ExceptionHandler>(descriptor, nullptr, dumpCallback, nullptr, true, -1);
    }

    virtual ~ServiceBase() = default;

    void run() {
        std::string appName = AppName();
        std::string appVersion = xstr(RELEASE_VERSION);
        LOG_I("Camera {} Starting service {} version {}", CAMERA_TYPE, appName, appVersion);
        while (running.load()) {
            MainLoop();
        }
        LOG_I("Service {} stopped", appName);
    }

    virtual void MainLoop() = 0;
    virtual void Initialize() = 0;

    void stop() {
        running.store(false);
    }

    std::string AppName() {
        int status = 0;
        const char* mangled = typeid(*this).name();
        char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
        return (status == 0 && demangled) ? demangled : mangled;
    }

   private:
    static ServiceBase* instance;
    std::atomic<bool> running;
    std::unique_ptr<google_breakpad::ExceptionHandler> exception_handler_;
    static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded) {
        printf("Dump path: %s\n", descriptor.path());
        return succeeded;
    }
};

inline ServiceBase* ServiceBase::instance = nullptr;