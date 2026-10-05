#include "SharedState.h"
#include <shellapi.h>
#include <cwctype>

using namespace ChiaKey::WindowsTsf;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count = 0;
    auto** args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args) return 2;
    std::wstring scope;
    bool stop = count == 2 && std::wstring(args[1]) == L"/stop";
    bool valid = count == 1 || stop;
    if (count == 3 && std::wstring(args[1]) == L"/test") {
        scope = args[2];
        valid = !scope.empty() && scope.size() <= 64 && scope.rfind(L".Test.", 0) == 0;
        for (const auto character : scope)
            if (!(iswalnum(character) || character == L'.' || character == L'-')) valid = false;
    }
    LocalFree(args);
    if (!valid) return 2;
    if (stop) { StopHistoryHost(); return 0; }
    // Logoff terminates this process and destroys its session-local kernel objects.
    // It never initializes the input engine, loads a lexicon, or creates a UI window.
    return RunHistoryHost(scope);
}
