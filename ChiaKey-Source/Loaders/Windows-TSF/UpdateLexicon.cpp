#include "UpdateLexicon.h"
#include <Windows.h>
#include <algorithm>
#include <ChiaKeyCore/ChiaKeyCore.h>

namespace {
std::string Utf8(const std::wstring& value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(),
                        size, nullptr, nullptr);
    return result;
}
bool SafeDirectory(const std::string& name) {
    if (name.empty() || name.size() > 150 || name.find("..") != std::string::npos ||
        name.front() < '0' || name.front() > '9' || name.back() == '.') return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    });
}
}

namespace ChiaKey::WindowsTsf {
std::vector<std::string> UpdateLexiconCandidates(const std::wstring& roaming) {
    std::vector<std::string> candidates;
    const std::wstring root = roaming + L"\\ChiaKeyUpdates\\Lexicons\\";
    HANDLE file = CreateFileW((root + L"active.txt").c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return candidates;
    char bytes[512]{};
    DWORD read = 0;
    const bool success = ReadFile(file, bytes, sizeof(bytes), &read, nullptr) != FALSE;
    CloseHandle(file);
    if (!success || read == sizeof(bytes)) return candidates;
    std::string pointer(bytes, read);
    size_t lines = 0;
    for (size_t start = 0; start < pointer.size() && lines++ < 2;) {
        const size_t end = pointer.find('\n', start);
        std::string name = pointer.substr(start, end == std::string::npos ? end : end - start);
        if (!name.empty() && name.back() == '\r') name.pop_back();
        if (SafeDirectory(name))
            candidates.push_back(Utf8(root) + "versions\\" + name + "\\ChiaKeySource.db");
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return candidates;
}
}

extern "C" int __stdcall ChiaKeyUpdatesValidateCore(const wchar_t* database, const wchar_t* temporary) {
    try {
        if (!database || !temporary) return 0;
        ChiaKey::RuntimePaths paths;
        paths.lexiconDatabasePath = Utf8(database);
        paths.writablePath = Utf8(temporary);
        std::string error;
        auto runtime = ChiaKey::Runtime::Create(paths, ChiaKey::EngineConfig(), &error);
        if (!runtime) return 0;
        auto engine = runtime->createEngine(&error);
        if (!engine) return 0;
        // Test the downloaded DB through the actual shipped Mandarin engine.
        for (char character : std::string("su3cl3")) {
            ChiaKey::KeyEvent key;
            key.keyCode = character;
            key.receivedString.assign(1, character);
            engine->handleKey(key);
        }
        if (engine->snapshot().composingText != "\xe4\xbd\xa0\xe5\xa5\xbd") return 0;
        engine->reset();
        return 1;
    } catch (...) {
        return 0;
    }
}
