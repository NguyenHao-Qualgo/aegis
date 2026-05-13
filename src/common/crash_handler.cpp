#include "aegis/common/crash_handler.hpp"
#include "aegis/common/config.hpp"

#include <cstdio>
#include <client/linux/handler/exception_handler.h>
#include <client/linux/handler/minidump_descriptor.h>

namespace aegis {
static google_breakpad::ExceptionHandler* g_handler = nullptr;

bool CrashHandler::OnMinidumpWritten(const google_breakpad::MinidumpDescriptor& descriptor,
                                     void* /*context*/,
                                     bool succeeded)
{
    if (succeeded) {
        LOG_E("Minidump written to: {}", descriptor.path());
    }
    else {
        LOG_E("Failed to write minidump");
    }
    return false;
}

CrashHandler::CrashHandler(const std::string& dump_path)
{
    google_breakpad::MinidumpDescriptor descriptor(dump_path);
    g_handler = new google_breakpad::ExceptionHandler(
        descriptor,
        nullptr,             // filter callback — nullptr means always write
        OnMinidumpWritten,
        nullptr,             // callback context
        true,                // install signal handlers
        -1                   // no out-of-process server fd
    );
}

CrashHandler::~CrashHandler()
{
    delete g_handler;
    g_handler = nullptr;
}

}  // namespace aegis