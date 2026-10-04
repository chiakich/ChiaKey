#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "ChiaKeyEngine.h"
#include "UpdateLexicon.h"
#include "OutputFilter.h"
#include "FrontendBehavior.h"
#include "PunctuationKeyboard.h"
#include "SharedState.h"
#include "NotificationWindow.h"
#include <thread>
#include <OVFileHelper.h>

using namespace ChiaKey::WindowsTsf;

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << std::endl;
        ++failures;
    }
}

KeyEvent Key(UINT virtualKey, bool shift = false, bool control = false) {
    KeyEvent event;
    event.virtualKey = virtualKey;
    event.shift = shift;
    event.control = control;
    return event;
}

bool Type(EngineSession& session, const char* keys) {
    for (const char* key = keys; *key; ++key) {
        const char upper = (*key >= 'a' && *key <= 'z') ? static_cast<char>(*key - 32) : *key;
        if (!session.handleKey(Key(static_cast<UINT>(upper))).handled) return false;
    }
    return true;
}

void TestLayout() {
    // U+20000 takes two UTF-16 units; ranges must not split it
    ChiaKey::EngineState state;
    state.composingText = "\xF0\xA0\x80\x80好";
    state.readingText = "ㄅ";
    state.cursorPosition = 1;
    EngineResult result = MakeResult(state);
    Check(result.compositionText == L"\U00020000ㄅ好", "reading goes in at the cursor");
    Check(result.compositionCursor == 3, "cursor counts UTF-16 units past the reading");
    Check(result.focusedSegment.start == 2 && result.focusedSegment.length == 1,
          "the reading is the focused segment while it is being typed");

    state.readingText.clear();
    state.cursorPosition = 2;
    state.wordSegments = {{0, 1}, {1, 1}};
    result = MakeResult(state);
    Check(result.focusedSegment.start == 2 && result.focusedSegment.length == 1,
          "the word before the cursor is focused");

    state.cursorPosition = 1;
    result = MakeResult(state);
    Check(result.focusedSegment.start == 0 && result.focusedSegment.length == 2,
          "a surrogate-pair word is focused whole");

    state.highlight = {0, 2};
    result = MakeResult(state);
    Check(result.focusedSegment.start == 0 && result.focusedSegment.length == 3,
          "a mark-mode highlight wins over word segments");

    state = {};
    state.candidateState.visible = true;
    state.candidateState.candidates = {"a", "b", "c", "d", "e"};
    state.candidateState.candidatesPerPage = 3;
    state.candidateState.currentPage = 1;
    state.candidateState.pageCount = 2;
    state.candidateState.selectionKeys = {"1", "2"};
    state.candidateState.highlightedIndex = 1;
    result = MakeResult(state);
    Check(result.candidates.size() == 2 && result.candidates[0].text == L"d" &&
              result.candidates[1].selectionKey == L"2" && result.highlightedCandidate == 1,
          "candidates are taken from the current page");
    Check(result.candidatePage == 2 && result.candidatePageCount == 2 &&
              result.candidatesPerPage == 3,
          "the page indicator is 1-based and the page keeps its full height");
}

void TestOutputConversion() {
    EngineResult direct;
    Check(ApplyFullWidthFallback(L' ', direct) && direct.handled && direct.committedText == L"　",
          "unhandled idle space becomes U+3000 in full-width mode");
    direct = {};
    Check(ApplyFullWidthFallback(L'A', direct) && direct.committedText == L"Ａ",
          "unhandled ASCII has a full-width fallback");
    direct = {};
    Check(!ApplyFullWidthFallback(L'\t', direct) && !ApplyFullWidthFallback(L'\r', direct) &&
              !direct.handled && direct.committedText.empty(),
          "editing keys cannot become direct full-width text");
    direct.compositionText = L"你好";
    Check(!ApplyFullWidthFallback(L' ', direct) && direct.compositionText == L"你好" &&
              direct.committedText.empty(),
          "fallback preserves a live composition");
    direct = {};
    direct.handled = true;
    direct.committedText = L"你好";
    Check(!ApplyFullWidthFallback(L' ', direct) && direct.committedText == L"你好",
          "fallback preserves engine selection and commits");
    const std::wstring traditional = L"千秋輸入法，臺灣測試繁體龍門";
    Check(FilterCommittedText(traditional, true) == L"千秋输入法，台湾测试繁体龙门",
          "simplified output uses the Mac conversion table");
    Check(FilterCommittedText(traditional, false) == traditional,
          "traditional output preserves the original text");
    Check(FilterCommittedText(L"ABC 123 ㄅ，。😀\U00020000測試", true) ==
              L"ABC 123 ㄅ，。😀\U00020000测试",
          "conversion preserves Latin, punctuation, Bopomofo, emoji and supplementary Han");
    Check(FilterCommittedText(L"", true).empty(), "empty output remains empty");
    Check(FilterCommittedText(L"汉语输入", true) == L"汉语输入",
          "already simplified text remains simplified");
    ChiaKey::EngineState state;
    state.composingText = "繁體";
    state.committedText = "繁體";
    state.candidateState.visible = true;
    state.candidateState.candidates = {"繁體"};
    state.candidateState.selectionKeys = {"1"};
    state.candidateState.candidatesPerPage = 1;
    state.candidateState.pageCount = 1;
    const EngineResult result = MakeResult(state);
    Check(result.compositionText == L"繁體" && !result.candidates.empty() &&
              result.candidates[0].text == L"繁體" &&
              FilterCommittedText(result.committedText, true) == L"繁体",
          "conversion leaves composition and candidates traditional");
}

void TestKeys() {
    KeyEvent shiftedComma = Key(VK_OEM_COMMA, true);
    ChiaKey::KeyEvent core = MakeCoreKey(shiftedComma);
    Check(core.keyCode == '<' && core.modifiers.shift, "keyCode carries the shifted character");

    KeyEvent numLockLetter = Key('S');
    numLockLetter.numLock = true;
    Check(!MakeCoreKey(numLockLetter).modifiers.numLock,
          "NumLock only reaches the engine on numpad keys");

    Check(IsInputMethodControlKey(Key(VK_OEM_COMMA, false, true)), "Ctrl+, is a punctuation chord");
    Check(!IsInputMethodControlKey(Key('C', false, true)), "Ctrl+C stays with the host");
    KeyEvent controlAlt = Key('A', false, true);
    controlAlt.alt = true;
    Check(IsInputMethodControlKey(controlAlt), "Ctrl+Alt+A is a punctuation chord");
}

void TestUpdatePointers(const char* writable) {
    const std::string base = std::string(writable) + "\\ChiaKeyUpdates";
    CreateDirectoryA(base.c_str(), nullptr);
    CreateDirectoryA((base + "\\Lexicons").c_str(), nullptr);
    const std::string pointer = base + "\\Lexicons\\active.txt";
    const std::wstring root(writable, writable + strlen(writable));
    Check(UpdateLexiconCandidates(root).empty(), "no external pointer uses bundled lexicon");
    { std::ofstream output(pointer, std::ios::binary); output << "2026.10.2-current\n2026.9.1-previous\n"; }
    auto candidates = UpdateLexiconCandidates(root);
    Check(candidates.size() == 2 && candidates[0].find("2026.10.2-current") != std::string::npos &&
              candidates[1].find("2026.9.1-previous") != std::string::npos,
          "current and previous pointers preserve priority");
    { std::ofstream output(pointer, std::ios::binary); output << "../escape\nC:\\outside\n2026.1-third\n"; }
    Check(UpdateLexiconCandidates(root).empty(), "unsafe and extra pointer lines cannot select a database");
    { std::ofstream output(pointer, std::ios::binary); output << "\n2026.9.1-previous\n"; }
    Check(UpdateLexiconCandidates(root).size() == 1, "previous pointer survives absent current pointer");
    { std::ofstream output(pointer, std::ios::binary); output << std::string(600, '1'); }
    Check(UpdateLexiconCandidates(root).empty(), "oversized activation pointer is rejected");
}

void TestFileTimestamps(const char* writable) {
    const std::string path = std::string(writable) + "/timestamp-test.txt";
    std::ofstream(path) << "timestamp fixture";
    const std::wstring wide = OpenVanilla::OVUTF16::FromUTF8(path);
    HANDLE file = CreateFileW(wide.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "timestamp fixture can be opened");
    if (file == INVALID_HANDLE_VALUE) return;
    const ULONGLONG epoch = 116444736000000000ULL;
    for (const ULONGLONG ticks : {epoch + 1700000000ULL * 10000000ULL + 1234567ULL,
                                  epoch, epoch - 1}) {
        ULARGE_INTEGER value;
        value.QuadPart = ticks;
        FILETIME time = {value.LowPart, value.HighPart};
        Check(SetFileTime(file, nullptr, nullptr, &time) != 0, "fixture write time can be set");
        const auto timestamp = OpenVanilla::OVPathHelper::TimestampForPath(path);
        const ULONGLONG unixTicks = ticks > epoch ? ticks - epoch : 0;
        Check(timestamp.timestamp() == static_cast<time_t>(unixTicks / 10000000ULL) &&
                  timestamp.subtimestamp() == static_cast<time_t>(unixTicks % 10000000ULL),
              "FILETIME uses Unix seconds and retains fractional ticks, clamping before epoch");
    }
    CloseHandle(file);
    DeleteFileW(wide.c_str());
}

void TestSession() {
    std::unique_ptr<EngineSession> session = EngineSession::Create();
    Check(session && session->ready(), "session is ready");
    if (!session || !session->ready()) return;

    Check(!session->wantsKey(Key(VK_SPACE)), "idle space stays with the host");
    Check(!session->wantsKey(Key(VK_TAB)), "idle Tab stays with the host");
    for (const auto& editing : std::vector<std::pair<UINT, std::wstring>>{
             {VK_TAB, L"\t"}, {VK_RETURN, L"\r"}, {VK_BACK, L"\b"}, {VK_ESCAPE, L"\x1b"}}) {
        KeyEvent event = Key(editing.first);
        event.text = editing.second;
        Check(!session->wantsKey(event), "idle editing text from ToUnicodeEx stays with the host");
        Check(MakeCoreKey(event).receivedString.empty(), "editing keys retain their core identity");
    }
    Check(!session->wantsKey(Key('2', false, true)), "Ctrl+2 without a composition stays with the host");
    Check(session->wantsKey(Key(VK_OEM_COMMA, false, true)),
          "Ctrl+, types punctuation without a composition");
    Check(!session->wantsKey(Key('0', false, true)), "Ctrl+0 without a composition stays with the host");
    Check(!session->wantsKey(Key('1', false, true)), "Ctrl+1 without a composition stays with the host");
    Check(Type(*session, "su"), "pre-space reading keys are handled");
    Check(session->wantsKey(Key(VK_SPACE)), "space while reading goes to the engine");
    EngineResult reading = session->handleKey(Key(VK_SPACE));
    Check(reading.handled && reading.committedText.empty() && !reading.compositionText.empty(),
          "space finishes a reading without inserting host whitespace");
    session->reset();
    Check(Type(*session, "su3cl3"), "你好 keys are handled");
    EngineResult result = session->handleKey(Key(VK_RIGHT));
    Check(result.compositionText == L"你好", "composes 你好");
    Check(result.compositionCursor == 2, "cursor at the end");
    Check(result.focusedSegment.length > 0, "a word is focused");
    Check(session->wantsKey(Key('2', false, true)), "Ctrl+2 in a composition goes to the engine");
    Check(!session->wantsKey(Key('C', false, true)), "Ctrl+C in a composition stays with the host");
    Check(session->wantsKey(Key(VK_OEM_COMMA, false, true)), "Ctrl+, in a composition goes to the engine");

    result = session->handleKey(Key(VK_SPACE));
    Check(result.candidatesVisible && !result.candidates.empty() &&
              result.candidates[0].selectionKey == L"1",
          "space opens candidates with selection keys");
    result = session->handleKey(Key(VK_ESCAPE));
    Check(!result.candidatesVisible, "Esc closes the candidate list");

    result = session->handleKey(Key(VK_RETURN));
    Check(result.committedText == L"你好", "Return commits 你好");
    Check(!session->hasComposition(), "nothing left after the commit");
    Check(!session->wantsKey(Key(VK_SPACE)), "space after a commit stays with the host");

    Check(Type(*session, "su3cl3"), "pre-Tab 你好 keys are handled");
    result = session->handleKey(Key(VK_LEFT));
    Check(result.compositionCursor == 1, "cursor moves between 你 and 好");
    Check(result.focusedSegment.length == 2, "你好 starts as one word");
    KeyEvent tab = Key(VK_TAB);
    tab.text = L"\t";  // ToUnicodeEx output from a real Windows Tab key
    Check(session->wantsKey(tab), "Tab in a composition goes to the engine");
    result = session->handleKey(tab);
    Check(result.handled && !result.beep && result.committedText.empty() &&
              result.compositionText == L"你好" && result.compositionCursor == 1 &&
              result.focusedSegment.length == 1,
          "Tab splits the word at the cursor without committing or inserting a tab");
    result = session->handleKey(tab);
    Check(result.handled && !result.beep && result.committedText.empty() &&
              result.compositionText == L"你好" && result.focusedSegment.length == 2,
          "a second Tab restores the word path");
    session->reset();

    result = session->handleKey(Key(VK_OEM_COMMA, true));
    Check(result.compositionText == L"，", "Shift+, composes a full-width comma");
    session->reset();

    KeyEvent controlAltSemicolon = Key(VK_OEM_1, false, true);
    controlAltSemicolon.alt = true;
    Check(session->wantsKey(controlAltSemicolon), "Ctrl+Alt+; goes to the engine");
    result = session->handleKey(controlAltSemicolon);
    Check(result.handled && (result.compositionText + result.committedText) == L"；",
          "Ctrl+Alt+; types a full-width semicolon");
    session->reset();
    Check(!session->hasComposition(), "reset drops the composition");
}

void WritePlist(const std::string& path, const std::string& body) {
    std::ofstream out(path, std::ios::trunc);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">\n<dict>\n"
        << body << "</dict>\n</plist>\n";
}

std::string Entry(const char* key, const char* value) {
    return std::string("\t<key>") + key + "</key>\n\t<string>" + value + "</string>\n";
}

void TestSettings(const std::string& writableDir) {
    const std::string preferences = writableDir + "\\Preferences";
    WritePlist(preferences + "\\Windows.plist",
               Entry("HighlightColor", "Green") +
                   Entry("ToggleInputMethodWithControlBackslash", "false") +
                   Entry("ShiftTogglesTemporaryEnglish", "false") +
                   Entry("EnablesCapsLockAsAlphanumericModeToggle", "true") +
                   Entry("ChineseConverterToggleKey", "k") +
                   Entry("RepeatLastCommitTextKey", "r") +
                   Entry("SoundFilename", "C:/Windows/Media/notify.wav") +
                   Entry("ShouldUseNotifyWindow", "false") +
                   Entry("KeyboardFormShouldFollowCursor", "true") +
                   Entry("UiLanguage", "en") +
                   "\t<key>ModulesSuppressedFromUI</key>\n\t<array>\n\t\t<string>Generic-simplex-cin"
                   "</string>\n\t\t<string>TraditionalMandarin</string>\n\t</array>\n");
    // the settings app writes the whole module plist, as the core does
    WritePlist(preferences + "\\SmartMandarin.plist",
               Entry("KeyboardLayout", "ETen") + Entry("CandidateSelectionKeys", "") +
                   Entry("ShowCandidateListWithSpace", "true"));

    std::unique_ptr<EngineSession> session = EngineSession::Create();
    const FrontendSettings frontend = CurrentFrontendSettings();
    Check(frontend.highlightColor == "Green", "a changed Windows.plist is reread");
    Check(frontend.uiLanguage == "en" && ReadEngineConfig(preferences, {}).locale == "en",
          "new runtimes use the selected UI locale, including module notifications");
    Check(!frontend.toggleWithControlBackslash, "boolean settings are read");
    Check(frontend.textColor == "White", "missing keys keep their defaults");
    Check(!frontend.shiftTogglesEnglish, "the Shift tap toggle can be turned off");
    Check(frontend.capsLockTogglesEnglish && frontend.chineseConverterToggleKey == "k" &&
              frontend.repeatLastCommitTextKey == "r" && !frontend.showNotifications && frontend.keyboardFollowsCursor &&
              frontend.soundFilename == "C:/Windows/Media/notify.wav",
          "legacy general and sound settings load from the same plist");
    Check(!frontend.simplifiedOutput, "simplified output defaults off");
    Check(frontend.suppressedInputMethods.size() == 2 &&
              frontend.suppressedInputMethods[0] == "Generic-simplex-cin",
          "the input methods hidden from the menu are read as an array");
    Check(SetSimplifiedOutput(true) && CurrentFrontendSettings().simplifiedOutput &&
              ReadFrontendSettings(preferences).simplifiedOutput,
          "menu toggle enables simplified output and saves it");
    Check(CurrentFrontendSettings().highlightColor == "Green" &&
              CurrentFrontendSettings().suppressedInputMethods.size() == 2,
          "output toggle preserves other frontend preferences");
    Check(SetSimplifiedOutput(false) && !ReadFrontendSettings(preferences).simplifiedOutput,
          "menu toggle restores traditional output and saves it");

    Type(*session, "su3cl3");
    Check(session->handleKey(Key(VK_RIGHT)).compositionText != L"你好",
          "a changed SmartMandarin.plist applies the keyboard layout");
    session->reset();

    WritePlist(preferences + "\\SmartMandarin.plist", Entry("KeyboardLayout", "Standard"));
    session = EngineSession::Create();
    Type(*session, "su3cl3");
    Check(session->handleKey(Key(VK_RIGHT)).compositionText == L"你好",
          "switching the layout back applies too");
    session->reset();

    // a restored backup keeps its old timestamp, older than what the engine last wrote
    WritePlist(preferences + "\\SmartMandarin.plist", Entry("KeyboardLayout", "ETen"));
    session = EngineSession::Create();
    WritePlist(preferences + "\\SmartMandarin.plist", Entry("KeyboardLayout", "Standard"));
    {
        HANDLE file = CreateFileA((preferences + "\\SmartMandarin.plist").c_str(),
                                  FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        FILETIME old{};
        SYSTEMTIME year2020{2020, 1, 3, 1, 0, 0, 0, 0};
        SystemTimeToFileTime(&year2020, &old);
        SetFileTime(file, nullptr, nullptr, &old);
        CloseHandle(file);
    }
    session = EngineSession::Create();
    Type(*session, "su3cl3");
    Check(session->handleKey(Key(VK_RIGHT)).compositionText == L"你好",
          "a plist restored with an older timestamp still applies");
    session->reset();

    const ChiaKey::EngineConfig config = ReadEngineConfig(preferences, ChiaKey::EngineConfig());
    Check(config.keyboardLayout == "Standard" && config.showCandidateListWithSpace,
          "a missing key falls back to the config passed in");
}

void TestGenericInputMethods() {
    Check(SelectInputMethod("Generic-cj-cin"), "Cangjie can be selected");
    std::unique_ptr<EngineSession> session = EngineSession::Create();
    Type(*session, "hapi");
    EngineResult result = session->handleKey(Key(VK_SPACE));
    if (result.committedText != L"的") {
        Check(result.candidatesVisible && !result.candidates.empty() && result.candidates[0].text == L"的",
              "Cangjie hapi offers 的");
        result = session->handleKey(Key('1'));
    }
    Check(result.committedText == L"的" || result.compositionText == L"的",
          "Cangjie hapi gives 的");
    session.reset();

    const auto methods = InputMethods();
    const bool userTable = std::any_of(methods.begin(), methods.end(), [](const auto& method) {
        return method.first == "Generic-test-cin" && method.second == L"測試";
    });
    Check(userTable, "a .cin under Tables/Generic is listed with its %cname");
    Check(SelectInputMethod("Generic-test-cin"), "the user table can be selected");
    session = EngineSession::Create();
    Type(*session, "ab");
    result = session->handleKey(Key(VK_SPACE));
    Check(result.committedText == L"測" || result.compositionText == L"測" ||
              (result.candidatesVisible && !result.candidates.empty() && result.candidates[0].text == L"測"),
          "the user table composes from its own chardef");
    session.reset();

    Check(SelectInputMethod("SmartMandarin"), "Smart Mandarin can be selected back");
}

void WriteUserTable(const std::string& writableDir) {
    CreateDirectoryA((writableDir + "\\Tables").c_str(), nullptr);
    CreateDirectoryA((writableDir + "\\Tables\\Generic").c_str(), nullptr);
    std::ofstream out(writableDir + "\\Tables\\Generic\\test.cin", std::ios::binary);
    out << "%gen_inp\n%ename TestTable\n%cname 測試\n%selkey 123456789\n"
           "%keyname begin\na 甲\nb 乙\n%keyname end\n"
           "%chardef begin\nab 測\n%chardef end\n";
}

}  // namespace

void TestSharedState(const std::string& writable) {
    const std::wstring scope = L".Test." + std::to_wstring(GetCurrentProcessId());
    SharedCommitHistory first(scope), second(scope);
    first.setStatusOwner(reinterpret_cast<HWND>(1234));
    Check(second.statusOwner() == reinterpret_cast<HWND>(1234), "active status ownership is shared between hosts");
    first.record(L"你好😀");
    Check(second.replay(false) == L"你好😀" && second.replay(true).empty(),
          "different TIP instances share actual output but refuse mid-composition replay");
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --history-child " + scope;
    STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
    if (CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &startup, &process)) {
        const DWORD wait = WaitForSingleObject(process.hProcess, 10000);
        DWORD code = 99; GetExitCodeProcess(process.hProcess, &code);
        Check(wait == WAIT_OBJECT_0 && code == 0 && first.replay(false) == L"另一個程序",
              "another process can update the session history");
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    } else Check(false, "launch session history child");
    const std::wstring huge(65537, L'甲');
    first.record(huge);
    Check(first.replay(false) == huge && second.replay(false).empty(),
          "oversized history cannot replay a truncated or stale commit in another instance");
    first.record(L"短文");
    Check(second.replay(false) == L"短文", "a later normal commit restores shared history");
    const std::string path = writable + "\\word-count-test";
    CreateDirectoryA(path.c_str(), nullptr);
    Check(ClearWordCounts(path), "clear independent word-count fixture");
    Check(CommittedCodePoints(L"你好😀\U00020000") == 4, "counts Unicode scalars, not UTF-16 units");
    Check(AddWordCount(path, L"旧", 94) && AddWordCount(path, L"你好", 100) &&
          AddWordCount(path, L"😀", 101), "persist counts by local calendar day");
    WordCounts counts;
    Check(ReadWordCounts(path, 101, &counts) && counts.today == 1 && counts.week == 3 && counts.total == 4,
          "rolling seven-day count excludes older days without dropping the total");
    Check(ReadWordCounts(path, 102, &counts) && counts.today == 0 && counts.week == 3 && counts.total == 4,
          "reading after midnight rolls today's count forward");
    bool a = false, b = false;
    std::thread writerA([&] { a = AddWordCount(path, L"甲乙", 102); });
    std::thread writerB([&] { b = AddWordCount(path, L"丙丁", 102); });
    writerA.join(); writerB.join();
    Check(a && b && ReadWordCounts(path, 102, &counts) && counts.today == 4 && counts.total == 8,
          "simultaneous hosts update counters transactionally");
    Check(ClearWordCounts(path) && ReadWordCounts(path, 102, &counts) && counts.total == 0 && counts.week == 0,
          "clear removes all persisted counts");
    Check(NotificationOpacity(999) == 255 && NotificationOpacity(1000) == 204 &&
          NotificationOpacity(1150) == 51 && NotificationOpacity(1200) == 0,
          "notification holds one second and fades by 20 percent every 50 ms");
    Check(NotificationAnimationOpacity(0) == 128 && NotificationAnimationOpacity(50) == 255 &&
          NotificationAnimationOpacity(1099) == 255 && NotificationAnimationOpacity(1100) == 204 &&
          NotificationAnimationOpacity(1300) == 0 && NotificationSlide(0) == -10 &&
          NotificationSlide(90) == -1 && NotificationSlide(100) == 0,
          "legacy notification slides in for 100 ms, holds, then fades");
    const auto firstNotice = NotificationRectangle({-1920, -100, 0, 1000}, 180, 95, 10, LONG_MIN);
    const auto nextNotice = NotificationRectangle({-1920, -100, 0, 1000}, 180, 95, 10, firstNotice.bottom);
    Check(firstNotice.left == -190 && firstNotice.top == -90 && nextNotice.top == 15,
          "notification stack starts at monitor top right and preserves negative coordinates");
    const auto wrappedNotice = NotificationRectangle({0, 0, 200, 150}, 180, 95, 10, 105);
    const auto tinyNotice = NotificationRectangle({0, 0, 80, 60}, 180, 95, 10, LONG_MIN);
    Check(wrappedNotice.top == 10 && tinyNotice.left == 0 && tinyNotice.top == 0 &&
          tinyNotice.right == 80 && tinyNotice.bottom == 60,
          "notification stack wraps and clamps to small work areas");
    Check(UiText(L"中文模式", "en") == L"Switch to Chinese mode." &&
          UiText(L"半形英數模式", "zh-CN") == L"半形英数模式",
          "mode notifications use the selected UI language");
    StatusWindowState state{true, -1000, 700};
    WriteStatusWindowState(state);
    const auto restored = ReadStatusWindowState();
    Check(restored.hasPosition && restored.left == -1000 && restored.top == 700,
          "floating bar position preserves negative monitor coordinates");
    Check(SetFrontendBool("ShouldUseMiniMode", true) && CurrentFrontendSettings().miniStatusBar &&
          SetFrontendBool("ShouldUseMiniMode", false) && !CurrentFrontendSettings().miniStatusBar,
          "mini mode shares the original preference key");
}

void TestPunctuationKeyboard() {
    KeyEvent open = Key(VK_OEM_COMMA, false, true); open.alt = true;
    Check(ShortcutFor(open, FrontendSettings{}) == FrontendShortcut::PunctuationKeyboard,
          "Ctrl+Alt+, opens the punctuation keyboard rather than a candidate list");
    open.shift = true;
    Check(ShortcutFor(open, FrontendSettings{}) == FrontendShortcut::None,
          "modified comma is not the keyboard command");
    Check(std::size(kPunctuationKeys) == 46 && PunctuationSymbol('1') == L'┌' &&
          PunctuationSymbol('Q') == L'├' && PunctuationSymbol('A') == L'└' &&
          PunctuationSymbol('Z') == L'─' && PunctuationSymbol(VK_OEM_PERIOD) == 0x2027 &&
          PunctuationSymbol(VK_OEM_COMMA) == 0xff0c && PunctuationSymbol(VK_OEM_5) == 0x300d,
          "keyboard keeps the historical four-row symbol map");
    Check(!PunctuationSymbol(VK_TAB) && !PunctuationSymbol(VK_RETURN) &&
          !PunctuationSymbol(VK_SPACE) && !PunctuationSymbol(VK_ESCAPE),
          "unmapped editing keys cannot insert host control characters");
    Check(PunctuationModifier(VK_SHIFT) && PunctuationModifier(VK_RCONTROL) &&
          !PunctuationModifier('A'), "releasing shortcut modifiers does not choose a symbol");
    KeyEvent direct; direct.directText = true; direct.text = L"，";
    Check(MakeCoreKey(direct).modifiers.directText && MakeCoreKey(direct).receivedString == "，",
          "keyboard symbol uses the original DirectText core path");
    SelectInputMethod("SmartMandarin");
    auto session = EngineSession::Create();
    Check(Type(*session, "su3cl3"), "compose nihao before keyboard symbol");
    const auto result = session->handleKey(direct);
    Check(result.handled && result.committedText.empty() && result.compositionText == L"你好，",
          "a physical keyboard symbol stays in the original sentence composition");
    session->reset();
}

void TestLegacyFrontendBehavior() {
    FrontendSettings settings;
    KeyEvent chord = Key(VK_OEM_5, false, true);
    Check(ShortcutFor(chord, settings) == FrontendShortcut::NextInputMethod,
          "legacy Ctrl+backslash cycles input methods, not language mode");
    settings.toggleWithControlBackslash = false;
    Check(ShortcutFor(chord, settings) == FrontendShortcut::None,
          "disabled cycling shortcut belongs to host");
    chord = Key('S', false, true); chord.alt = true;
    Check(ShortcutFor(chord, settings) == FrontendShortcut::ToggleSimplified,
          "legacy Ctrl+Alt+S toggles Chinese output conversion");
    chord.virtualKey = 'G';
    Check(ShortcutFor(chord, settings) == FrontendShortcut::RepeatCommit,
          "legacy Ctrl+Alt+G repeats last commit");
    settings.repeatLastCommitTextKey = "r";
    Check(ShortcutFor(chord, settings) == FrontendShortcut::None,
          "changing repeat shortcut releases the old chord");
    chord.virtualKey = 'R';
    Check(ShortcutFor(chord, settings) == FrontendShortcut::RepeatCommit,
          "custom repeat letter is recognized by virtual key identity");
    chord.shift = true;
    Check(ShortcutFor(chord, settings) == FrontendShortcut::None, "shifted host chord is not intercepted");
    chord.shift = false; settings.repeatLastCommitTextKey.clear();
    Check(ShortcutFor(chord, settings) == FrontendShortcut::None, "empty repeat letter disables shortcut");
    chord.virtualKey = 'S'; chord.alt = false;
    Check(ShortcutFor(chord, settings) == FrontendShortcut::None, "Ctrl+S remains Save in the host");

    const std::vector<std::pair<std::string, std::wstring>> methods = {
        {"SmartMandarin", L"好打注音"}, {"TraditionalMandarin", L"傳統注音"},
        {"Generic-cj-cin", L"倉頡"}, {"Generic-simplex-cin", L"簡易"}};
    settings.suppressedInputMethods = {"TraditionalMandarin"};
    Check(NextInputMethod("SmartMandarin", methods, settings) == "Generic-cj-cin",
          "cycling skips suppressed methods");
    Check(NextInputMethod("Generic-simplex-cin", methods, settings) == "SmartMandarin",
          "cycling wraps around in menu order");
    Check(NextInputMethod("removed", methods, settings) == "SmartMandarin",
          "missing selected method recovers to first visible method");
    settings.suppressedInputMethods = {"TraditionalMandarin", "SmartMandarin", "Generic-cj-cin", "Generic-simplex-cin"};
    Check(NextInputMethod("SmartMandarin", methods, settings) == "SmartMandarin" &&
          NextInputMethod("SmartMandarin", {}, settings) == "SmartMandarin",
          "empty and fully hidden menus preserve the current method");

    KeyEvent latin = Key('A'); latin.capsLock = true; latin.text = L"A";
    Check(!CapsLockAlphanumeric(latin, settings), "Caps Lock mode is opt in");
    settings.capsLockTogglesEnglish = true;
    Check(CapsLockAlphanumeric(latin, settings) && AlphanumericCharacter(latin, settings) == L'a',
          "Caps Lock English mode removes uppercase latch");
    latin.shift = true; latin.text = L"a";
    Check(AlphanumericCharacter(latin, settings) == L'A', "Shift still produces uppercase in Caps Lock English mode");
    latin.text = L" ";
    Check(AlphanumericCharacter(latin, settings) == L' ', "Caps Lock preserves halfwidth space");
    latin.text = L"\t";
    Check(AlphanumericCharacter(latin, settings) == 0, "Caps Lock does not insert an editing key as text");
    latin.text = L"A"; latin.control = true;
    Check(AlphanumericCharacter(latin, settings) == 0, "Caps Lock preserves host Ctrl shortcuts");

    const COLORREF fallback = RGB(140, 91, 156);
    Check(CustomColor("Color -15584170", fallback) == RGB(0x12, 0x34, 0x56),
          "legacy signed ARGB custom color keeps RGB channel order");
    for (const char* bad : {"Color ", "Color nope", "Color 2147483648", "Color -2147483649", "Color 42junk", "Color +42"})
        Check(CustomColor(bad, fallback) == fallback, "malformed custom color retains default");

    CommitHistory history;
    Check(history.replay(false).empty(), "no history produces no text");
    history.record(L"你好"); history.record(L"");
    Check(history.replay(false) == L"你好" && history.replay(true).empty(),
          "repeat preserves last nonempty commit and refuses a live composition");
    history.record(L"台湾");
    Check(history.replay(false) == L"台湾", "repeat retains actual converted output");
    ChiaKey::EngineState notification;
    notification.notifications = {"saved"};
    Check(MakeResult(notification, true).notification == L"saved" &&
          MakeResult(notification, false).notification.empty() && MakeResult(notification, true).message.empty(), "notification window can be disabled");
    notification.tooltip = "reading hint";
    Check(MakeResult(notification, false).message == L"reading hint",
          "disabling notifications retains candidate and reverse-lookup hints");
}

void TestReverseLookup(const ChiaKey::RuntimePaths& initialPaths) {
    auto paths = initialPaths;
    paths.writablePath += "\\reverse-lookup";
    CreateDirectoryA(paths.writablePath.c_str(), nullptr);
    std::string error;
    auto runtime = ChiaKey::Runtime::Create(paths, ChiaKey::EngineConfig(), &error);
    Check(runtime != nullptr, "reverse lookup runtime loads official lexicon");
    if (!runtime) { std::cerr << error << std::endl; return; }
    const auto methods = runtime->reverseLookupMethods();
    for (const char* method : {"ReverseLookup-Generic-cj-cin", "ReverseLookup-Mandarin-bpmf-cin",
                             "ReverseLookup-Mandarin-bpmf-cin-HanyuPinyin"}) {
        Check(std::any_of(methods.begin(), methods.end(), [method](const auto& item) {
            return item.first == method;
        }), "legacy reverse lookup choice is backed by an initialized module");
    }
    runtime->setAssociatedPhrasesEnabled(true);
    Check(runtime->setReverseLookupMethod("ReverseLookup-Mandarin-bpmf-cin"), "Bopomofo lookup can be enabled");
    Check(!runtime->setReverseLookupMethod("ReverseLookup-nonexistent") &&
          runtime->reverseLookupMethod() == "ReverseLookup-Mandarin-bpmf-cin",
          "unknown lookup does not destroy the selected filter");
    auto engine = runtime->createEngine(&error);
    Check(engine != nullptr, "lookup engine can be created");
    if (!engine) return;
    for (char character : std::string("su3cl3")) engine->handleAsciiKey(character);
    engine->handleKey(MakeCoreKey(Key(VK_RETURN)));
    auto state = engine->snapshot();
    Check(state.committedText == "你好" && state.tooltip.find("ㄋ") != std::string::npos,
          "committed Chinese text reports Bopomofo without changing output");
    engine->acknowledgeCommit();
    Check(runtime->setReverseLookupMethod("ReverseLookup-Mandarin-bpmf-cin-HanyuPinyin"), "Pinyin lookup replaces Bopomofo");
    for (char character : std::string("su3cl3")) engine->handleAsciiKey(character);
    engine->handleKey(MakeCoreKey(Key(VK_RETURN)));
    state = engine->snapshot();
    Check(state.committedText == "你好" && state.tooltip.find("ni") != std::string::npos,
          "Pinyin lookup reports Latin readings without changing output");
    Check(runtime->setReverseLookupMethod("") && runtime->reverseLookupMethod().empty() &&
          runtime->associatedPhrasesEnabled(), "disabling lookup preserves unrelated associated-phrase filter");
}

void TestSymbols() {
    const std::vector<SymbolPage> pages = SymbolPages();
    Check(!pages.empty() && pages.front().buttons && !pages.front().entries.empty(),
          "the symbol table starts with a page of buttons");
    Check(!pages.empty() && pages.front().entries.front().tooltip.find(L"U+") != std::wstring::npos,
          "a symbol button's tooltip shows its code point");
    Check(std::any_of(pages.begin(), pages.end(), [](const SymbolPage& page) { return !page.buttons; }),
          "the symbol table has a page of canned messages");

    SymbolWindowState state;
    state.visible = true;
    state.page = L"顏文字";
    state.hasPosition = true;
    state.left = -1200;
    state.bottom = 900;
    WriteSymbolWindowState(state);
    const SymbolWindowState read = ReadSymbolWindowState();
    Check(read.visible && read.page == state.page && read.hasPosition && read.left == -1200 &&
              read.bottom == 900,
          "the symbol window's state survives a round trip");
}

int main(int argc, char* argv[]) {
    if (argc == 3 && std::string(argv[1]) == "--history-child") {
        const std::string scope(argv[2]);
        SharedCommitHistory history(std::wstring(scope.begin(), scope.end()));
        if (history.replay(false) != L"你好😀") return 1;
        history.record(L"另一個程序"); return 0;
    }
    if (argc < 4) {
        std::cerr << "usage: chiakey_tsf_engine_test <source-dir> <writable-dir> <lexicon>"
                  << std::endl;
        return 2;
    }
    ChiaKey::RuntimePaths paths;
    paths.loadedPath = argv[1];
    paths.resourcePath = argv[1];
    paths.writablePath = argv[2];
    paths.lexiconDatabasePath = argv[3];
    CreateDirectoryA(argv[2], nullptr);
    // user tables are only scanned when the runtime starts
    WriteUserTable(argv[2]);
    std::string error;
    if (!InitializeRuntime(paths, &error)) {
        std::cerr << "runtime: " << error << std::endl;
        return 1;
    }

    const auto methods = InputMethods();
    Check(methods.size() >= 4 && methods[0].first == "SmartMandarin" &&
              methods[1].second == L"傳統注音" && methods[2].first == "Generic-cj-cin" &&
              methods[3].second == L"簡易",
          "the input methods are listed in the mac menu's order and names");
    TestSharedState(argv[2]);
    Check(UiText(L"傳統注音", "en") == L"Traditional Phonetic" &&
          UiText(L"詞彙編輯器…", "zh-CN") == L"词汇编辑器…", "native menu labels follow the selected UI language");
    TestLayout();
    TestOutputConversion();
    TestLegacyFrontendBehavior();
    TestPunctuationKeyboard();
    TestKeys();
    TestUpdatePointers(argv[2]);
    TestFileTimestamps(argv[2]);
    TestSession();
    TestSettings(argv[2]);
    TestGenericInputMethods();
    TestSymbols();
    TestReverseLookup(paths);
    if (failures) return 1;
    std::cout << "chiakey_tsf_engine_test: OK" << std::endl;
    return 0;
}
