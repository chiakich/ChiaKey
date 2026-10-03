#include "ChiaKeyEngine.h"

#include <AclAPI.h>
#include <ShlObj.h>
#include <sddl.h>

#include <algorithm>
#include <mutex>

#include "Diagnostics.h"
#include "UpdateLexicon.h"
#include "OpenVanilla.h"
#include "PlainVanilla.h"

namespace ChiaKey::WindowsTsf {
namespace {

using OpenVanilla::OVKeyCode;
using OpenVanilla::OVKeyValueMap;
using OpenVanilla::OVPathHelper;
using OpenVanilla::PVPlistValue;
using OpenVanilla::PVPropertyList;

constexpr char kSmartMandarinPlist[] = "SmartMandarin.plist";
constexpr char kFrontendPlist[] = "Windows.plist";
// the phrase editor advances it on every commit (ChiaKeyUserPhraseCoordination.h)
constexpr char kUserPhrasesDirtyFlag[] = "SmartMandarinUserData.dirty";
// in Preferences but not a module's: the rest are, user tables included
constexpr const wchar_t* kNonModulePlists[] = {
    L"SmartMandarin.plist", L"Windows.plist", L"Loader.plist", L"SymbolWindow.plist",
};

bool BoolValue(OVKeyValueMap& map, const char* key, bool fallback) {
    return map.hasKey(key) ? map.isKeyTrue(key) : fallback;
}

std::string StringValue(OVKeyValueMap& map, const char* key, const std::string& fallback) {
    return map.hasKey(key) ? map.stringValueForKey(key) : fallback;
}

bool SameConfig(const ChiaKey::EngineConfig& a, const ChiaKey::EngineConfig& b) {
    return a.keyboardLayout == b.keyboardLayout &&
           a.candidateSelectionKeys == b.candidateSelectionKeys &&
           a.candidateCursorAtEndOfTargetBlock == b.candidateCursorAtEndOfTargetBlock &&
           a.showCandidateListWithSpace == b.showCandidateListWithSpace &&
           a.clearComposingTextWithEsc == b.clearComposingTextWithEsc &&
           a.shiftKeyAlwaysCommitUppercaseCharacters ==
               b.shiftKeyAlwaysCommitUppercaseCharacters &&
           a.composingTextBufferSize == b.composingTextBufferSize;
}

std::wstring Utf8ToWide(const std::string& text);

// A write time moves once per clock tick, about 15 ms, so a rewrite right after
// the engine's own can keep it; the size usually tells the two apart.
struct FileStamp {
    FILETIME time{};
    ULONGLONG size = 0;
};

FileStamp Stamp(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    FileStamp stamp;
    if (!GetFileAttributesExW(Utf8ToWide(path).c_str(), GetFileExInfoStandard, &data)) {
        return stamp;
    }
    stamp.time = data.ftLastWriteTime;
    stamp.size = (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    return stamp;
}

bool SameStamp(const FileStamp& a, const FileStamp& b) {
    return CompareFileTime(&a.time, &b.time) == 0 && a.size == b.size;
}

// one value for every module plist, so an edit, a new table's plist or a removal all show
ULONGLONG ModulePlistsStamp(const std::string& preferencesPath) {
    ULONGLONG stamp = 14695981039346656037ull;
    const auto mix = [&stamp](ULONGLONG value) {
        stamp = (stamp ^ value) * 1099511628211ull;
    };
    WIN32_FIND_DATAW found{};
    HANDLE search = FindFirstFileW((Utf8ToWide(preferencesPath) + L"\\*.plist").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) return stamp;
    do {
        const bool skipped = std::any_of(
            std::begin(kNonModulePlists), std::end(kNonModulePlists),
            [&found](const wchar_t* name) { return _wcsicmp(name, found.cFileName) == 0; });
        if (skipped) continue;
        for (const wchar_t* character = found.cFileName; *character; ++character) mix(*character);
        mix((static_cast<ULONGLONG>(found.ftLastWriteTime.dwHighDateTime) << 32) |
            found.ftLastWriteTime.dwLowDateTime);
        mix((static_cast<ULONGLONG>(found.nFileSizeHigh) << 32) | found.nFileSizeLow);
    } while (FindNextFileW(search, &found));
    FindClose(search);
    return stamp;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

// The core counts code points; TSF ranges count UTF-16 units.
std::vector<std::wstring> SplitCodePoints(const std::string& utf8) {
    const std::wstring wide = Utf8ToWide(utf8);
    std::vector<std::wstring> result;
    for (size_t index = 0; index < wide.size();) {
        const size_t units = IS_HIGH_SURROGATE(wide[index]) && index + 1 < wide.size() &&
                                     IS_LOW_SURROGATE(wide[index + 1])
                                 ? 2
                                 : 1;
        result.emplace_back(wide, index, units);
        index += units;
    }
    return result;
}

std::wstring ModuleDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&ModuleDirectory), &module)) {
        return {};
    }
    std::wstring path(32768, L'\0');
    const DWORD length =
        GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    path.resize(length);
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring() : path.substr(0, separator);
}

std::wstring RoamingFolder() {
    PWSTR path = nullptr;
    std::wstring result;
    // an AppContainer fails the default lookup, which verifies the folder it cannot see
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DONT_VERIFY, nullptr,
                                       &path))) {
        result = path;
    }
    CoTaskMemFree(path);
    if (result.empty()) {
        wchar_t buffer[MAX_PATH + 1]{};
        const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH + 1);
        if (length && length <= MAX_PATH) result.assign(buffer, length);
    }
    return result;
}

bool IsAppContainer() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD isAppContainer = 0;
    DWORD returned = 0;
    const BOOL queried = GetTokenInformation(token, TokenIsAppContainer, &isAppContainer,
                                             sizeof(isAppContainer), &returned);
    CloseHandle(token);
    return queried && isAppContainer;
}

// Store apps and Start/Search run in an AppContainer
void GrantAppContainerAccess(const std::wstring& directory) {
    PSID packages = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-15-2-1", &packages)) return;
    PACL current = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetNamedSecurityInfoW(directory.c_str(), SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION, nullptr, nullptr, &current,
                              nullptr, &descriptor) == ERROR_SUCCESS) {
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE |
                                      FILE_GENERIC_EXECUTE | DELETE;
        access.grfAccessMode = GRANT_ACCESS;
        access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        access.Trustee.ptstrName = static_cast<LPWSTR>(packages);
        PACL updated = nullptr;
        if (SetEntriesInAclW(1, &access, current, &updated) == ERROR_SUCCESS) {
            SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()), SE_FILE_OBJECT,
                                  DACL_SECURITY_INFORMATION, nullptr, nullptr, updated,
                                  nullptr);
            LocalFree(updated);
        }
        LocalFree(descriptor);
    }
    LocalFree(packages);
}

bool HasLowIntegrityLabel(const std::wstring& directory) {
    PACL sacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (GetNamedSecurityInfoW(directory.c_str(), SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION,
                              nullptr, nullptr, nullptr, &sacl,
                              &descriptor) != ERROR_SUCCESS) {
        return false;
    }
    bool low = false;
    for (DWORD index = 0; sacl && index < sacl->AceCount; ++index) {
        ACE_HEADER* header = nullptr;
        if (GetAce(sacl, index, reinterpret_cast<void**>(&header)) &&
            header->AceType == SYSTEM_MANDATORY_LABEL_ACE_TYPE) {
            auto* label = reinterpret_cast<SYSTEM_MANDATORY_LABEL_ACE*>(header);
            low = *GetSidSubAuthority(&label->SidStart, 0) <= SECURITY_MANDATORY_LOW_RID;
        }
    }
    LocalFree(descriptor);
    return low;
}

// AppContainers run at low integrity; to them an unlabeled (medium) file is read-only
void SetLowIntegrityLabel(const std::wstring& directory) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"S:(ML;OICI;NW;;;LW)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return;
    }
    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL sacl = nullptr;
    if (GetSecurityDescriptorSacl(descriptor, &present, &sacl, &defaulted) && present) {
        SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()), SE_FILE_OBJECT,
                              LABEL_SECURITY_INFORMATION, nullptr, nullptr, nullptr, sacl);
    }
    LocalFree(descriptor);
}

// the label doubles as the marker, so a configured directory is not rewritten per process
void ShareWithAppContainers(const std::wstring& directory) {
    if (HasLowIntegrityLabel(directory)) return;
    GrantAppContainerAccess(directory);
    SetLowIntegrityLabel(directory);
}

ChiaKey::RuntimePaths DefaultPaths(const std::wstring& writableRoot, bool shareWithAppContainers) {
    const std::wstring moduleDirectory = ModuleDirectory();
    ChiaKey::RuntimePaths paths;
    paths.loadedPath = WideToUtf8(moduleDirectory);
    paths.resourcePath = paths.loadedPath;
    // the installer puts the 32-bit DLL in a subfolder and ships the lexicon once
    std::wstring lexicon = moduleDirectory + L"\\ChiaKeySource.db";
    if (GetFileAttributesW(lexicon.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const size_t separator = moduleDirectory.find_last_of(L"\\/");
        if (separator != std::wstring::npos) {
            lexicon = moduleDirectory.substr(0, separator) + L"\\ChiaKeySource.db";
        }
    }
    paths.lexiconDatabasePath = WideToUtf8(lexicon);
    if (!writableRoot.empty()) {
        const std::wstring writable = writableRoot + L"\\ChiaKey";
        CreateDirectoryW(writable.c_str(), nullptr);
        if (shareWithAppContainers) ShareWithAppContainers(writable);
        paths.writablePath = WideToUtf8(writable);
    }
    return paths;
}

struct RuntimeHolder {
    std::mutex mutex;
    std::shared_ptr<ChiaKey::Runtime> runtime;
    std::string preferencesPath;
    FileStamp smartStamp;
    ULONGLONG moduleStamp = 0;
    std::string userPhrasesDirtyFlag;
    FileStamp userPhrasesStamp;
    FileStamp frontendStamp;
    FrontendSettings frontend;
    ULONGLONG nextRefresh = 0;
    // an AppContainer running on its temp folder until the shared one is usable
    bool privateFallback = false;
    ULONGLONG nextSharedRetry = 0;
    std::vector<std::string> lexiconCandidates;
    std::string lexiconDatabasePath;
    bool explicitPaths = false;
};

// leaked on purpose: no teardown under the loader lock; each Engine saves its own learning
RuntimeHolder& Holder() {
    static RuntimeHolder* holder = new RuntimeHolder();
    return *holder;
}

// the leaked runtime keeps SQLite handles open, so COM must never unload the DLL under it
void PinModule() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       reinterpret_cast<LPCWSTR>(&PinModule), &module);
}

constexpr ULONGLONG kRefreshIntervalMilliseconds = 1000;
constexpr ULONGLONG kSharedRetryIntervalMilliseconds = 60000;

std::string PreferencesPath(const std::string& writablePath) {
    return OVPathHelper::PathCat(writablePath, "Preferences");
}

// Create() writes its config back over the module plist, so it gets the saved one
std::shared_ptr<ChiaKey::Runtime> CreateRuntime(const ChiaKey::RuntimePaths& paths,
                                                std::string* error, bool allowUpdates = true) {
    // The helper validates before publication; still handle missing/unreadable/corrupt
    // external files here. AppContainer fallback must never prevent offline input.
    for (const auto& candidate : allowUpdates ? UpdateLexiconCandidates(RoamingFolder()) : std::vector<std::string>()) {
        ChiaKey::RuntimePaths external = paths;
        external.lexiconDatabasePath = candidate;
        auto runtime = ChiaKey::Runtime::Create(
            external, ReadEngineConfig(PreferencesPath(paths.writablePath), ChiaKey::EngineConfig()), error);
        if (runtime) {
            Holder().lexiconDatabasePath = candidate;
            return runtime;
        }
        Trace("Updated lexicon rejected: %s", error ? error->c_str() : "unknown error");
    }
    auto runtime = ChiaKey::Runtime::Create(
        paths, ReadEngineConfig(PreferencesPath(paths.writablePath), ChiaKey::EngineConfig()),
        error);
    if (runtime) Holder().lexiconDatabasePath = paths.lexiconDatabasePath;
    return runtime;
}

// only real differences: setConfig rewrites the plist and would wake every other process
void RefreshSettingsLocked(RuntimeHolder& holder, bool force) {
    if (!holder.runtime || holder.preferencesPath.empty()) return;
    holder.nextRefresh = GetTickCount64() + kRefreshIntervalMilliseconds;
    const std::string smartPath = OVPathHelper::PathCat(holder.preferencesPath, kSmartMandarinPlist);
    const std::string frontendPath = OVPathHelper::PathCat(holder.preferencesPath, kFrontendPlist);

    const ULONGLONG moduleStamp = ModulePlistsStamp(holder.preferencesPath);
    // a fresh runtime has just read them all
    const bool moduleChanged = !force && moduleStamp != holder.moduleStamp;
    holder.moduleStamp = moduleStamp;
    const bool smartChanged = !force && !SameStamp(Stamp(smartPath), holder.smartStamp);
    if (force || moduleChanged || smartChanged) {
        const ChiaKey::EngineConfig current = holder.runtime->config();
        const ChiaKey::EngineConfig wanted = ReadEngineConfig(holder.preferencesPath, current);
        // setConfig also resyncs the modules, which picks up keys EngineConfig does not carry,
        // and rewrites a plist only when its values differ
        if (moduleChanged || smartChanged || !SameConfig(current, wanted)) {
            holder.runtime->setConfig(wanted);
        }
        holder.smartStamp = Stamp(smartPath);
    }

    const FileStamp phrasesStamp = Stamp(holder.userPhrasesDirtyFlag);
    if (!force && !SameStamp(phrasesStamp, holder.userPhrasesStamp)) {
        holder.runtime->reloadUserPhrases();
    }
    holder.userPhrasesStamp = phrasesStamp;

    const FileStamp frontendStamp = Stamp(frontendPath);
    if (force || !SameStamp(frontendStamp, holder.frontendStamp)) {
        holder.frontend = ReadFrontendSettings(holder.preferencesPath);
        if (holder.runtime->associatedPhrasesEnabled() != holder.frontend.associatedPhrases) {
            holder.runtime->setAssociatedPhrasesEnabled(holder.frontend.associatedPhrases);
        }
        holder.frontendStamp = frontendStamp;
    }
}

void AdoptLocked(RuntimeHolder& holder, std::shared_ptr<ChiaKey::Runtime> runtime,
                 const std::string& writablePath, bool privateFallback) {
    holder.runtime = std::move(runtime);
    if (!holder.runtime) return;
    PinModule();
    holder.preferencesPath = PreferencesPath(writablePath);
    holder.userPhrasesDirtyFlag = OVPathHelper::PathCat(writablePath, kUserPhrasesDirtyFlag);
    holder.privateFallback = privateFallback;
    holder.lexiconCandidates = UpdateLexiconCandidates(RoamingFolder());
    holder.nextSharedRetry = GetTickCount64() + kSharedRetryIntervalMilliseconds;
    RefreshSettingsLocked(holder, true);
}

// a cheap check before the full Runtime::Create, which opens the lexicon
bool SharedDirectoryIsWritable(const std::wstring& roaming) {
    if (roaming.empty()) return false;
    const std::wstring probe = roaming + L"\\ChiaKey\\.chiakey-appcontainer-probe";
    HANDLE file = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    return true;
}

// once a desktop app has shared the directory, later engines move onto it
void RetrySharedRuntimeLocked(RuntimeHolder& holder) {
    if (!holder.privateFallback || GetTickCount64() < holder.nextSharedRetry) return;
    holder.nextSharedRetry = GetTickCount64() + kSharedRetryIntervalMilliseconds;
    const std::wstring roaming = RoamingFolder();
    if (!SharedDirectoryIsWritable(roaming)) return;
    std::string error;
    ChiaKey::RuntimePaths paths = DefaultPaths(roaming, false);
    if (auto runtime = CreateRuntime(paths, &error)) {
        AdoptLocked(holder, std::move(runtime), paths.writablePath, false);
    } else {
        Trace("Runtime::Create (AppContainer, shared retry) failed: %s", error.c_str());
    }
}

std::shared_ptr<ChiaKey::Runtime> CreateDefaultRuntime(std::string* writablePath,
                                                       bool* privateFallback) {
    std::string error;
    *privateFallback = false;
    const std::wstring roaming = RoamingFolder();
    if (!IsAppContainer()) {
        ChiaKey::RuntimePaths paths = DefaultPaths(roaming, true);
        if (auto runtime = CreateRuntime(paths, &error)) {
            *writablePath = paths.writablePath;
            return runtime;
        }
        Trace("Runtime::Create failed: %s", error.c_str());
        return nullptr;
    }

    // until a desktop app has created the shared directory, a private one keeps typing working
    std::string sharedError = "no roaming folder";
    if (!roaming.empty()) {
        ChiaKey::RuntimePaths paths = DefaultPaths(roaming, false);
        if (auto runtime = CreateRuntime(paths, &error)) {
            *writablePath = paths.writablePath;
            return runtime;
        }
        sharedError = WideToUtf8(roaming) + ": " + error;
        Trace("Runtime::Create (AppContainer, shared) failed: %s", sharedError.c_str());
    }
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    if (!length || length > MAX_PATH) return nullptr;
    std::wstring privateRoot(temp, length);
    if (!privateRoot.empty() && privateRoot.back() == L'\\') privateRoot.pop_back();
    ChiaKey::RuntimePaths paths = DefaultPaths(privateRoot, false);
    if (auto runtime = CreateRuntime(paths, &error)) {
        *writablePath = paths.writablePath;
        *privateFallback = true;
        // debug output does not reach a listener from an AppContainer, so leave the reason here
        const std::wstring note = privateRoot + L"\\ChiaKey\\fallback.txt";
        HANDLE file = CreateFileW(note.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(file, sharedError.data(), static_cast<DWORD>(sharedError.size()), &written,
                      nullptr);
            CloseHandle(file);
        }
        return runtime;
    }
    Trace("Runtime::Create (AppContainer, private) failed: %s", error.c_str());
    return nullptr;
}

// throttled callers are on the key path; the rest refresh on every call
std::shared_ptr<ChiaKey::Runtime> SharedRuntime(bool throttled = false) {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    // a failed attempt is retried on the next activation, e.g. after install
    if (!holder.runtime) {
        std::string writablePath;
        bool privateFallback = false;
        auto runtime = CreateDefaultRuntime(&writablePath, &privateFallback);
        AdoptLocked(holder, std::move(runtime), writablePath, privateFallback);
    } else if (!throttled || GetTickCount64() >= holder.nextRefresh) {
        RetrySharedRuntimeLocked(holder);
        const auto candidates = holder.explicitPaths ? holder.lexiconCandidates : UpdateLexiconCandidates(RoamingFolder());
        if (candidates != holder.lexiconCandidates) {
            std::string writablePath;
            bool privateFallback = false;
            if (auto runtime = CreateDefaultRuntime(&writablePath, &privateFallback)) {
                AdoptLocked(holder, std::move(runtime), writablePath, privateFallback);
            }
        }
        RefreshSettingsLocked(holder, false);
    }
    return holder.runtime;
}

int SpecialKeyCode(UINT virtualKey) {
    switch (virtualKey) {
        case VK_BACK: return OVKeyCode::Backspace;
        case VK_DELETE: return OVKeyCode::Delete;
        case VK_UP: return OVKeyCode::Up;
        case VK_DOWN: return OVKeyCode::Down;
        case VK_LEFT: return OVKeyCode::Left;
        case VK_RIGHT: return OVKeyCode::Right;
        case VK_HOME: return OVKeyCode::Home;
        case VK_END: return OVKeyCode::End;
        case VK_PRIOR: return OVKeyCode::PageUp;
        case VK_NEXT: return OVKeyCode::PageDown;
        case VK_TAB: return OVKeyCode::Tab;
        case VK_ESCAPE: return OVKeyCode::Esc;
        case VK_SPACE: return OVKeyCode::Space;
        case VK_RETURN: return OVKeyCode::Return;
        default: return 0;
    }
}

// Bopomofo is positional; ToUnicodeEx reports localized characters under a Chinese TIP
char PrintableAsciiFromVirtualKey(const KeyEvent& event) {
    if (event.virtualKey >= 'A' && event.virtualKey <= 'Z') {
        const bool upper = event.shift != event.capsLock;
        return static_cast<char>((upper ? 'A' : 'a') + event.virtualKey - 'A');
    }
    if (event.virtualKey >= '0' && event.virtualKey <= '9') {
        static constexpr char shiftedDigits[] = ")!@#$%^&*(";
        return event.shift ? shiftedDigits[event.virtualKey - '0']
                           : static_cast<char>(event.virtualKey);
    }
    if (event.virtualKey >= VK_NUMPAD0 && event.virtualKey <= VK_NUMPAD9 &&
        event.numLock) {
        return static_cast<char>('0' + event.virtualKey - VK_NUMPAD0);
    }
    switch (event.virtualKey) {
        case VK_SPACE: return ' ';
        case VK_OEM_1: return event.shift ? ':' : ';';
        case VK_OEM_PLUS: return event.shift ? '+' : '=';
        case VK_OEM_COMMA: return event.shift ? '<' : ',';
        case VK_OEM_MINUS: return event.shift ? '_' : '-';
        case VK_OEM_PERIOD: return event.shift ? '>' : '.';
        case VK_OEM_2: return event.shift ? '?' : '/';
        case VK_OEM_3: return event.shift ? '~' : '`';
        case VK_OEM_4: return event.shift ? '{' : '[';
        case VK_OEM_5: return event.shift ? '|' : '\\';
        case VK_OEM_6: return event.shift ? '}' : ']';
        case VK_OEM_7: return event.shift ? '"' : '\'';
        default: return 0;
    }
}

bool IsNavigationOrEditingKey(UINT key) {
    return SpecialKeyCode(key) != 0;
}

bool IsPunctuationListKey(const KeyEvent& event) {
    return event.control && !event.alt && !event.shift &&
           (event.virtualKey == '0' || event.virtualKey == '1');
}

bool IsQuickUserPhraseKey(const KeyEvent& event) {
    return event.control && !event.alt && !event.shift && event.virtualKey >= '1' &&
           event.virtualKey <= '9';
}

}  // namespace

bool IsInputMethodControlKey(const KeyEvent& event) {
    if (!event.control) return false;

    const char character = PrintableAsciiFromVirtualKey(event);
    if (event.alt) {
        // _ctrl_opt_a through _ctrl_opt_z, plus these five
        if (event.virtualKey >= 'A' && event.virtualKey <= 'Z') return true;
        switch (character) {
            case ';': case '\'': case ',': case '.': case '/':
                return true;
            default:
                return false;
        }
    }

    switch (character) {
        case ',': case '.': case '<': case '>': case '?':
        case ';': case ':': case '\'': case '"': case '[': case ']':
        // Ctrl+0 and Ctrl+1 open _punctuation_list
        case '0': case '1':
            return true;
        default:
            return false;
    }
}

ChiaKey::KeyEvent MakeCoreKey(const KeyEvent& event) {
    ChiaKey::KeyEvent key;
    key.modifiers.alt = event.alt;
    key.modifiers.ctrl = event.control;
    key.modifiers.shift = event.shift;
    key.modifiers.capsLock = event.capsLock;
    // the module rejects main-keyboard Bopomofo keys that carry NumLock
    key.modifiers.numLock = event.numLock && event.virtualKey >= VK_NUMPAD0 &&
                            event.virtualKey <= VK_DIVIDE;
    key.keyCode = SpecialKeyCode(event.virtualKey);
    // ToUnicodeEx can return text for Tab/Return. Editing keys must keep their
    // identity rather than being replaced by the keyboard layout's output.
    if (key.keyCode && event.virtualKey != VK_SPACE) return key;

    if (event.control && event.virtualKey >= 'A' && event.virtualKey <= 'Z') {
        key.keyCode = 'a' + static_cast<int>(event.virtualKey - 'A');
        key.receivedString.assign(1, static_cast<char>(key.keyCode));
    } else if (const char character = PrintableAsciiFromVirtualKey(event)) {
        key.keyCode = static_cast<unsigned char>(character);
        key.receivedString.assign(1, character);
    } else if (event.text.size() == 1 && event.text.front() >= 32 &&
               event.text.front() < 127) {
        key.keyCode = static_cast<int>(event.text.front());
        key.receivedString.assign(1, static_cast<char>(key.keyCode));
    } else if (!event.text.empty()) {
        key.receivedString = WideToUtf8(event.text);
    }
    return key;
}

EngineResult MakeResult(const ChiaKey::EngineState& state) {
    EngineResult result;
    result.beep = state.beeped;
    result.committedText = Utf8ToWide(state.committedText);

    const std::vector<std::wstring> composing = SplitCodePoints(state.composingText);
    const std::vector<std::wstring> reading = SplitCodePoints(state.readingText);
    const size_t cursor = std::min(state.cursorPosition, composing.size());

    // The reading is shown at the cursor, as PVCombinedUTF16TextBuffer does.
    std::vector<std::wstring> display(composing.begin(), composing.begin() + cursor);
    display.insert(display.end(), reading.begin(), reading.end());
    display.insert(display.end(), composing.begin() + cursor, composing.end());
    std::vector<LONG> offsets{0};
    for (const std::wstring& codePoint : display) {
        result.compositionText += codePoint;
        offsets.push_back(static_cast<LONG>(result.compositionText.size()));
    }
    result.compositionCursor = offsets[cursor + reading.size()];

    // composing positions at or after the cursor sit behind the reading
    const auto displayIndex = [&](size_t position, bool isEnd) {
        const bool shifted = isEnd ? position > cursor : position >= cursor;
        return std::min(position + (shifted ? reading.size() : 0), display.size());
    };
    size_t from = 0;
    size_t to = 0;
    if (!reading.empty()) {
        from = cursor;
        to = cursor + reading.size();
    } else if (state.highlight.length) {
        from = displayIndex(state.highlight.location, false);
        to = displayIndex(state.highlight.location + state.highlight.length, true);
    } else if (!composing.empty()) {
        const size_t target = cursor ? cursor - 1 : 0;
        for (const ChiaKey::TextRange& segment : state.wordSegments) {
            if (target >= segment.location && target < segment.location + segment.length) {
                from = displayIndex(segment.location, false);
                to = displayIndex(segment.location + segment.length, true);
                break;
            }
        }
    }
    if (to > from) {
        result.focusedSegment.start = offsets[from];
        result.focusedSegment.length = offsets[to] - offsets[from];
    }

    const ChiaKey::CandidateState& panel = state.candidateState;
    if (panel.visible && !panel.selectionKeys.empty()) {
        const size_t first = panel.currentPage * panel.candidatesPerPage;
        for (size_t index = 0; index < panel.selectionKeys.size() &&
                               first + index < panel.candidates.size();
             ++index) {
            result.candidates.push_back({Utf8ToWide(panel.selectionKeys[index]),
                                         Utf8ToWide(panel.candidates[first + index])});
        }
        result.candidatesVisible = !result.candidates.empty();
        result.highlightedCandidate = panel.highlightedIndex;
        result.candidatesPerPage = std::max(panel.candidatesPerPage, result.candidates.size());
        result.candidatePage = panel.currentPage + 1;
        result.candidatePageCount = std::max<size_t>(panel.pageCount, 1);
    }

    result.message = Utf8ToWide(state.tooltip);
    if (result.message.empty() && !state.notifications.empty()) {
        result.message = Utf8ToWide(state.notifications.back());
    }
    return result;
}

ChiaKey::EngineConfig ReadEngineConfig(const std::string& preferencesPath,
                                       ChiaKey::EngineConfig config) {
    const std::string path = OVPathHelper::PathCat(preferencesPath, kSmartMandarinPlist);
    if (!OVPathHelper::PathExists(path)) return config;
    // the map points into the plist's own dictionary, so the plist has to outlive it
    PVPropertyList plist(path);
    OVKeyValueMap map = plist.rootDictionary()->keyValueMap();
    config.keyboardLayout = StringValue(map, "KeyboardLayout", config.keyboardLayout);
    config.candidateSelectionKeys =
        StringValue(map, "CandidateSelectionKeys", config.candidateSelectionKeys);
    config.candidateCursorAtEndOfTargetBlock = BoolValue(
        map, "CandidateCursorAtEndOfTargetBlock", config.candidateCursorAtEndOfTargetBlock);
    config.showCandidateListWithSpace =
        BoolValue(map, "ShowCandidateListWithSpace", config.showCandidateListWithSpace);
    config.clearComposingTextWithEsc =
        BoolValue(map, "ClearComposingTextWithEsc", config.clearComposingTextWithEsc);
    config.shiftKeyAlwaysCommitUppercaseCharacters =
        BoolValue(map, "ShiftKeyAlwaysCommitUppercaseCharacters",
                  config.shiftKeyAlwaysCommitUppercaseCharacters);
    if (map.hasKey("ComposingTextBufferSize")) {
        const int size = map.intValueForKey("ComposingTextBufferSize");
        if (size > 0) config.composingTextBufferSize = static_cast<size_t>(size);
    }
    return config;
}

FrontendSettings ReadFrontendSettings(const std::string& preferencesPath) {
    FrontendSettings settings;
    const std::string path = OVPathHelper::PathCat(preferencesPath, kFrontendPlist);
    if (!OVPathHelper::PathExists(path)) return settings;
    PVPropertyList plist(path);
    OVKeyValueMap map = plist.rootDictionary()->keyValueMap();
    settings.highlightColor = StringValue(map, "HighlightColor", settings.highlightColor);
    settings.backgroundColor = StringValue(map, "BackgroundColor", settings.backgroundColor);
    settings.textColor = StringValue(map, "TextColor", settings.textColor);
    settings.backgroundPattern = BoolValue(map, "BackgroundPattern", settings.backgroundPattern);
    settings.playSoundOnTypingError =
        BoolValue(map, "ShouldPlaySoundOnTypingError", settings.playSoundOnTypingError);
    settings.toggleWithControlBackslash = BoolValue(
        map, "ToggleInputMethodWithControlBackslash", settings.toggleWithControlBackslash);
    settings.shiftTogglesEnglish =
        BoolValue(map, "ShiftTogglesTemporaryEnglish", settings.shiftTogglesEnglish);
    settings.associatedPhrases =
        BoolValue(map, "EnableAssociatedPhrases", settings.associatedPhrases);
    settings.simplifiedOutput = BoolValue(map, "SimplifiedOutput", settings.simplifiedOutput);
    if (PVPlistValue* hidden = plist.rootDictionary()->valueForKey("ModulesSuppressedFromUI")) {
        for (size_t index = 0; index < hidden->arraySize(); ++index) {
            PVPlistValue* identifier = hidden->arrayElementAtIndex(index);
            if (identifier && identifier->type() == PVPlistValue::String) {
                settings.suppressedInputMethods.push_back(identifier->stringValue());
            }
        }
    }
    return settings;
}

void RefreshSettings() { SharedRuntime(); }

FrontendSettings CurrentFrontendSettings() {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    return holder.frontend;
}

bool SetSimplifiedOutput(bool enabled) {
    SharedRuntime();
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    if (holder.preferencesPath.empty()) return false;
    PVPropertyList plist(OVPathHelper::PathCat(holder.preferencesPath, kFrontendPlist));
    OVKeyValueMap map = plist.rootDictionary()->keyValueMap();
    map.setKeyBoolValue("SimplifiedOutput", enabled);
    plist.write();
    holder.frontend = ReadFrontendSettings(holder.preferencesPath);
    holder.frontendStamp = Stamp(OVPathHelper::PathCat(holder.preferencesPath, kFrontendPlist));
    return holder.frontend.simplifiedOutput == enabled;
}

std::wstring SettingsAppPath() {
    const std::wstring directory = ModuleDirectory();
    std::wstring settings = directory + L"\\ChiaKeySettings.exe";
    // A 32-bit application on x64 Windows loads the DLL in the x86 subfolder;
    // the desktop settings/updater lives alongside the main 64-bit DLL.
    if (GetFileAttributesW(settings.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const size_t separator = directory.find_last_of(L"\\/");
        if (separator != std::wstring::npos)
            settings = directory.substr(0, separator) + L"\\ChiaKeySettings.exe";
    }
    return settings;
}

ChiaKey::RuntimePaths DesktopRuntimePaths() {
    SharedRuntime();
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    auto paths = DefaultPaths(RoamingFolder(), true);
    if (!holder.lexiconDatabasePath.empty()) paths.lexiconDatabasePath = holder.lexiconDatabasePath;
    return paths;
}

bool InitializeRuntime(const ChiaKey::RuntimePaths& paths, std::string* errorMessage) {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    if (holder.runtime) return true;
    holder.explicitPaths = true;
    AdoptLocked(holder, CreateRuntime(paths, errorMessage, false), paths.writablePath, false);
    return holder.runtime != nullptr;
}

namespace {

constexpr char kSymbolWindowPlist[] = "SymbolWindow.plist";

// the name, the code points, then the description, as the mac symbol window shows them
std::wstring SymbolTooltip(const ChiaKey::SymbolItem& item) {
    std::wstring codePoints;
    for (const std::wstring& codePoint : SplitCodePoints(item.text)) {
        unsigned value = codePoint[0];
        if (codePoint.size() == 2) {
            value = 0x10000 + ((value - 0xD800) << 10) + (codePoint[1] - 0xDC00u);
        }
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"%sU+%04X", codePoints.empty() ? L"" : L" ", value);
        codePoints += buffer;
    }
    std::wstring tooltip = Utf8ToWide(item.name);
    if (!tooltip.empty()) tooltip += L"\n";
    tooltip += codePoints;
    if (!item.description.empty()) tooltip += L"\n" + Utf8ToWide(item.description);
    return tooltip;
}

std::string SymbolWindowPlistPath() {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    return holder.preferencesPath.empty()
               ? std::string()
               : OVPathHelper::PathCat(holder.preferencesPath, kSymbolWindowPlist);
}

}  // namespace

std::vector<SymbolPage> SymbolPages() {
    std::vector<SymbolPage> pages;
    const auto runtime = SharedRuntime();
    if (!runtime) return pages;
    for (const ChiaKey::SymbolCategory& category : runtime->symbolCategories()) {
        SymbolPage page;
        page.name = Utf8ToWide(category.name);
        page.buttons = category.buttons;
        for (const ChiaKey::SymbolItem& item : category.items) {
            SymbolEntry entry;
            entry.text = Utf8ToWide(item.text);
            // edit controls only break a line on CR LF
            for (size_t at = entry.text.find(L'\n'); at != std::wstring::npos;
                 at = entry.text.find(L'\n', at + 2)) {
                entry.text.insert(at, 1, L'\r');
            }
            entry.label = Utf8ToWide(item.label);
            if (category.buttons) entry.tooltip = SymbolTooltip(item);
            page.entries.push_back(std::move(entry));
        }
        pages.push_back(std::move(page));
    }
    return pages;
}

std::wstring UserCannedMessagesPath() {
    const auto runtime = SharedRuntime();
    return runtime ? Utf8ToWide(runtime->userCannedMessagesPath()) : std::wstring();
}

SymbolWindowState ReadSymbolWindowState() {
    SymbolWindowState state;
    const std::string path = SymbolWindowPlistPath();
    if (path.empty() || !OVPathHelper::PathExists(path)) return state;
    PVPropertyList plist(path);
    OVKeyValueMap map = plist.rootDictionary()->keyValueMap();
    state.visible = BoolValue(map, "Visible", false);
    state.page = Utf8ToWide(StringValue(map, "Page", std::string()));
    state.hasPosition = map.hasKey("Left") && map.hasKey("Bottom");
    if (state.hasPosition) {
        state.left = map.intValueForKey("Left");
        state.bottom = map.intValueForKey("Bottom");
    }
    return state;
}

void WriteSymbolWindowState(const SymbolWindowState& state) {
    const std::string path = SymbolWindowPlistPath();
    if (path.empty()) return;
    PVPropertyList plist(path);
    OVKeyValueMap map = plist.rootDictionary()->keyValueMap();
    map.setKeyBoolValue("Visible", state.visible);
    map.setKeyStringValue("Page", WideToUtf8(state.page));
    if (state.hasPosition) {
        map.setKeyIntValue("Left", state.left);
        map.setKeyIntValue("Bottom", state.bottom);
    }
    plist.write();
}

std::string CurrentInputMethod() {
    const auto runtime = SharedRuntime();
    return runtime ? runtime->primaryInputMethod() : std::string();
}

bool SelectInputMethod(const std::string& identifier) {
    const auto runtime = SharedRuntime();
    return runtime && runtime->setPrimaryInputMethod(identifier);
}

std::vector<std::pair<std::string, std::wstring>> InputMethods() {
    // the mac menu's names and order (CVApplicationController); the rest are user tables
    static const std::pair<const char*, const wchar_t*> kKnown[] = {
        {"SmartMandarin", L"好打注音"},
        {"TraditionalMandarin", L"傳統注音"},
        {"Generic-cj-cin", L"倉頡"},
        {"Generic-simplex-cin", L"簡易"},
    };
    std::vector<std::pair<std::string, std::wstring>> result;
    const auto runtime = SharedRuntime();
    if (!runtime) return result;
    const auto available = runtime->inputMethods();
    for (const auto& known : kKnown) {
        for (const auto& entry : available) {
            if (entry.first == known.first) result.emplace_back(entry.first, known.second);
        }
    }
    for (const auto& entry : available) {
        const bool listed = std::any_of(std::begin(kKnown), std::end(kKnown),
                                        [&](const auto& known) { return entry.first == known.first; });
        if (!listed) result.emplace_back(entry.first, Utf8ToWide(entry.second));
    }
    return result;
}

std::unique_ptr<EngineSession> EngineSession::Create() {
    std::unique_ptr<ChiaKey::Engine> engine;
    if (const auto runtime = SharedRuntime()) {
        std::string error;
        engine = runtime->createEngine(&error);
        if (!engine) Trace("createEngine failed: %s", error.c_str());
    }
    return std::unique_ptr<EngineSession>(new EngineSession(std::move(engine)));
}

bool EngineSession::hasComposition() const { return engine_ && engine_->isComposing(); }

bool EngineSession::wantsKey(const KeyEvent& event) const {
    if (!engine_) return false;
    const bool composing = hasComposition();
    // Idle half-width spaces belong to the host, not the candidate engine.
    // TextService handles explicit full-width mode before asking wantsKey.
    if (!composing && event.virtualKey == VK_SPACE) return false;
    // Ctrl+1..9 marks the last N composed characters as a user phrase
    if (IsQuickUserPhraseKey(event)) return composing;
    // punctuation chords always type; Ctrl+0/1 is left to app zoom and tab keys when idle
    if (IsInputMethodControlKey(event)) return composing || !IsPunctuationListKey(event);
    if (event.control || event.alt) return false;
    if (composing && IsNavigationOrEditingKey(event.virtualKey)) return true;
    // declining here makes TSF skip OnKeyDown and hand the raw key to the app
    return PrintableAsciiFromVirtualKey(event) != 0 || !event.text.empty();
}

EngineResult EngineSession::handleKey(const KeyEvent& event) {
    if (!engine_) return {};
    // applying settings can rebuild the context, so never mid-composition
    if (!hasComposition()) {
        const auto runtime = SharedRuntime(true);
        if (runtime && runtime != engine_->runtime()) {
            std::string error;
            if (auto fresh = runtime->createEngine(&error)) {
                engine_ = std::move(fresh);
            } else {
                Trace("createEngine on the new runtime failed: %s", error.c_str());
            }
        }
    }
    const bool handled = engine_->handleKey(MakeCoreKey(event));
    EngineResult result = MakeResult(engine_->snapshot());
    result.handled = handled;
    if (!result.committedText.empty()) engine_->acknowledgeCommit();
    return result;
}

void EngineSession::reset() {
    if (engine_) engine_->reset();
}

}  // namespace ChiaKey::WindowsTsf
