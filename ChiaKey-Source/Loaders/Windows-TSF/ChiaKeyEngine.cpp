#include "ChiaKeyEngine.h"

#include <AclAPI.h>
#include <ShlObj.h>
#include <sddl.h>

#include <algorithm>
#include <mutex>

#include "Diagnostics.h"
#include "OpenVanilla.h"
#include "PlainVanilla.h"

namespace ChiaKey::WindowsTsf {
namespace {

using OpenVanilla::OVKeyCode;
using OpenVanilla::OVKeyValueMap;
using OpenVanilla::OVPathHelper;
using OpenVanilla::PVPropertyList;

constexpr char kSmartMandarinPlist[] = "SmartMandarin.plist";
constexpr char kFrontendPlist[] = "Windows.plist";
// the modules the settings app edits besides Smart Mandarin
constexpr const char* kOtherModulePlists[] = {
    "TraditionalMandarin.plist",
    "Generic-cj-cin.plist",
    "Generic-simplex-cin.plist",
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

FILETIME Stamp(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(Utf8ToWide(path).c_str(), GetFileExInfoStandard, &data)) return {};
    return data.ftLastWriteTime;
}

bool SameStamp(const FILETIME& a, const FILETIME& b) { return CompareFileTime(&a, &b) == 0; }

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
    paths.lexiconDatabasePath = WideToUtf8(moduleDirectory + L"\\ChiaKeySource.db");
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
    FILETIME smartStamp{};
    FILETIME otherStamps[std::size(kOtherModulePlists)]{};
    FILETIME frontendStamp{};
    FrontendSettings frontend;
    ULONGLONG nextRefresh = 0;
};

// leaked on purpose: no teardown under the loader lock; each Engine saves its own learning
RuntimeHolder& Holder() {
    static RuntimeHolder* holder = new RuntimeHolder();
    return *holder;
}

constexpr ULONGLONG kRefreshIntervalMilliseconds = 1000;

std::string PreferencesPath(const std::string& writablePath) {
    return OVPathHelper::PathCat(writablePath, "Preferences");
}

// Create() writes its config back over the module plist, so it gets the saved one
std::shared_ptr<ChiaKey::Runtime> CreateRuntime(const ChiaKey::RuntimePaths& paths,
                                                std::string* error) {
    return ChiaKey::Runtime::Create(
        paths, ReadEngineConfig(PreferencesPath(paths.writablePath), ChiaKey::EngineConfig()),
        error);
}

// only real differences: setConfig rewrites the plist and would wake every other process
void RefreshSettingsLocked(RuntimeHolder& holder, bool force) {
    if (!holder.runtime || holder.preferencesPath.empty()) return;
    holder.nextRefresh = GetTickCount64() + kRefreshIntervalMilliseconds;
    const std::string smartPath = OVPathHelper::PathCat(holder.preferencesPath, kSmartMandarinPlist);
    const std::string frontendPath = OVPathHelper::PathCat(holder.preferencesPath, kFrontendPlist);

    bool otherChanged = false;
    for (size_t index = 0; index < std::size(kOtherModulePlists); ++index) {
        const FILETIME stamp =
            Stamp(OVPathHelper::PathCat(holder.preferencesPath, kOtherModulePlists[index]));
        // a fresh runtime has just read them all
        if (!force && !SameStamp(stamp, holder.otherStamps[index])) otherChanged = true;
        holder.otherStamps[index] = stamp;
    }
    if (force || otherChanged || !SameStamp(Stamp(smartPath), holder.smartStamp)) {
        const ChiaKey::EngineConfig current = holder.runtime->config();
        const ChiaKey::EngineConfig wanted = ReadEngineConfig(holder.preferencesPath, current);
        // setConfig also resyncs the active module, which picks up the other modules' edits
        if (otherChanged || !SameConfig(current, wanted)) holder.runtime->setConfig(wanted);
        holder.smartStamp = Stamp(smartPath);
    }

    const FILETIME frontendStamp = Stamp(frontendPath);
    if (force || !SameStamp(frontendStamp, holder.frontendStamp)) {
        holder.frontend = ReadFrontendSettings(holder.preferencesPath);
        if (holder.runtime->associatedPhrasesEnabled() != holder.frontend.associatedPhrases) {
            holder.runtime->setAssociatedPhrasesEnabled(holder.frontend.associatedPhrases);
        }
        holder.frontendStamp = frontendStamp;
    }
}

void AdoptLocked(RuntimeHolder& holder, std::shared_ptr<ChiaKey::Runtime> runtime,
                 const std::string& writablePath) {
    holder.runtime = std::move(runtime);
    if (!holder.runtime) return;
    holder.preferencesPath = PreferencesPath(writablePath);
    RefreshSettingsLocked(holder, true);
}

std::shared_ptr<ChiaKey::Runtime> CreateDefaultRuntime(std::string* writablePath) {
    std::string error;
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
        AdoptLocked(holder, CreateDefaultRuntime(&writablePath), writablePath);
    } else if (!throttled || GetTickCount64() >= holder.nextRefresh) {
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
    settings.associatedPhrases =
        BoolValue(map, "EnableAssociatedPhrases", settings.associatedPhrases);
    return settings;
}

void RefreshSettings() { SharedRuntime(); }

FrontendSettings CurrentFrontendSettings() {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    return holder.frontend;
}

std::wstring SettingsAppPath() { return ModuleDirectory() + L"\\ChiaKeySettings.exe"; }

bool InitializeRuntime(const ChiaKey::RuntimePaths& paths, std::string* errorMessage) {
    RuntimeHolder& holder = Holder();
    std::lock_guard<std::mutex> lock(holder.mutex);
    if (holder.runtime) return true;
    AdoptLocked(holder, CreateRuntime(paths, errorMessage), paths.writablePath);
    return holder.runtime != nullptr;
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
    // Ctrl+1..9 marks the last N composed characters as a user phrase
    if (IsQuickUserPhraseKey(event)) return composing;
    // TextService checks the punctuation chords itself
    if (event.control || event.alt) return false;
    if (composing && IsNavigationOrEditingKey(event.virtualKey)) return true;
    // declining here makes TSF skip OnKeyDown and hand the raw key to the app
    return PrintableAsciiFromVirtualKey(event) != 0 || !event.text.empty();
}

EngineResult EngineSession::handleKey(const KeyEvent& event) {
    if (!engine_) return {};
    // applying settings can rebuild the context, so never mid-composition
    if (!hasComposition()) SharedRuntime(true);
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
