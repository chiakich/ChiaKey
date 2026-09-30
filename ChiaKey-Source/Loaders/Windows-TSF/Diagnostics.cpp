#include "Diagnostics.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>

namespace ChiaKey::WindowsTsf {

void Trace(const char* format, ...) {
    char message[1024]{};
    va_list arguments;
    va_start(arguments, format);
    const int length = vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (length <= 0) return;

    char line[1100]{};
    snprintf(line, sizeof(line), "ChiaKeyTsf pid=%lu tid=%lu %s\n", GetCurrentProcessId(),
             GetCurrentThreadId(), message);
    OutputDebugStringA(line);
}

}  // namespace ChiaKey::WindowsTsf
