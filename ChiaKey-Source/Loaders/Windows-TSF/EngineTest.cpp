#include <iostream>
#include <string>

#include "ChiaKeyEngine.h"

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
    state.candidateState.selectionKeys = {"1", "2"};
    state.candidateState.highlightedIndex = 1;
    result = MakeResult(state);
    Check(result.candidates.size() == 2 && result.candidates[0].text == L"d" &&
              result.candidates[1].selectionKey == L"2" && result.highlightedCandidate == 1,
          "candidates are taken from the current page");
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

void TestSession() {
    std::unique_ptr<EngineSession> session = EngineSession::Create();
    Check(session && session->ready(), "session is ready");
    if (!session || !session->ready()) return;

    Check(!session->wantsKey(Key('2', false, true)), "Ctrl+2 without a composition stays with the host");
    Check(Type(*session, "su3cl3"), "你好 keys are handled");
    EngineResult result = session->handleKey(Key(VK_RIGHT));
    Check(result.compositionText == L"你好", "composes 你好");
    Check(result.compositionCursor == 2, "cursor at the end");
    Check(result.focusedSegment.length > 0, "a word is focused");
    Check(session->wantsKey(Key('2', false, true)), "Ctrl+2 in a composition goes to the engine");
    Check(!session->wantsKey(Key('C', false, true)), "Ctrl+C in a composition stays with the host");

    result = session->handleKey(Key(VK_SPACE));
    Check(result.candidatesVisible && !result.candidates.empty() &&
              result.candidates[0].selectionKey == L"1",
          "space opens candidates with selection keys");
    result = session->handleKey(Key(VK_ESCAPE));
    Check(!result.candidatesVisible, "Esc closes the candidate list");

    result = session->handleKey(Key(VK_RETURN));
    Check(result.committedText == L"你好", "Return commits 你好");
    Check(!session->hasComposition(), "nothing left after the commit");

    result = session->handleKey(Key(VK_OEM_COMMA, true));
    Check(result.compositionText == L"，", "Shift+, composes a full-width comma");
    session->reset();
    Check(!session->hasComposition(), "reset drops the composition");
}

}  // namespace

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
    std::string error;
    if (!InitializeRuntime(paths, &error)) {
        std::cerr << "runtime: " << error << std::endl;
        return 1;
    }

    TestLayout();
    TestKeys();
    TestSession();
    if (failures) return 1;
    std::cout << "chiakey_tsf_engine_test: OK" << std::endl;
    return 0;
}
