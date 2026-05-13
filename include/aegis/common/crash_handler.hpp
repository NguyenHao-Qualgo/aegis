#pragma once

#include <string>
#include <client/linux/handler/minidump_descriptor.h>

namespace aegis {

class CrashHandler {
public:
    explicit CrashHandler(const std::string& dump_path);
    ~CrashHandler();

    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

private:
    static bool OnMinidumpWritten(const google_breakpad::MinidumpDescriptor& descriptor,
                                  void* context,
                                  bool succeeded);
};

}  // namespace aegis