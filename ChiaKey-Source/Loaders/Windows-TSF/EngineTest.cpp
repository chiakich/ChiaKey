#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "ChiaKeyEngine.h"
#include "UpdateLexicon.h"
#include "OutputFilter.h"
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
                   "\t<key>ModulesSuppressedFromUI</key>\n\t<array>\n\t\t<string>Generic-simplex-cin"
                   "</string>\n\t\t<string>TraditionalMandarin</string>\n\t</array>\n");
    // the settings app writes the whole module plist, as the core does
    WritePlist(preferences + "\\SmartMandarin.plist",
               Entry("KeyboardLayout", "ETen") + Entry("CandidateSelectionKeys", "") +
                   Entry("ShowCandidateListWithSpace", "true"));

    std::unique_ptr<EngineSession> session = EngineSession::Create();
    const FrontendSettings frontend = CurrentFrontendSettings();
    Check(frontend.highlightColor == "Green", "a changed Windows.plist is reread");
    Check(!frontend.toggleWithControlBackslash, "boolean settings are read");
    Check(frontend.textColor == "White", "missing keys keep their defaults");
    Check(!frontend.shiftTogglesEnglish, "the Shift tap toggle can be turned off");
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
    TestLayout();
    TestOutputConversion();
    TestKeys();
    TestUpdatePointers(argv[2]);
    TestFileTimestamps(argv[2]);
    TestSession();
    TestSettings(argv[2]);
    TestGenericInputMethods();
    TestSymbols();
    if (failures) return 1;
    std::cout << "chiakey_tsf_engine_test: OK" << std::endl;
    return 0;
}
