#pragma once

#include <Windows.h>
#include <ctffunc.h>
#include <msctf.h>
#include <wrl/client.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "CandidateWindow.h"
#include "ChiaKeyEngine.h"
#include "FrontendBehavior.h"
#include "SymbolWindow.h"
#include "PunctuationKeyboard.h"
#include "SharedState.h"
#include "NotificationWindow.h"
#include "UIElement.h"
#include "ReloadControl.h"

namespace ChiaKey::WindowsTsf {

class LangBarButton;

class TextService final : public ITfTextInputProcessorEx,
                          public ITfKeyEventSink,
                          public ITfCompositionSink,
                          public ITfTextEditSink,
                          public ITfThreadMgrEventSink,
                          public ITfThreadFocusSink,
                          public ITfCompartmentEventSink,
                          public ITfDisplayAttributeProvider,
                          public ITfFunctionProvider,
                          public ITfFnConfigure,
                          public IChiaKeyReloadControl {
public:
    static HRESULT CreateInstance(IUnknown* outer, REFIID iid, void** object);

    TextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor / ITfTextInputProcessorEx
    STDMETHODIMP Activate(ITfThreadMgr* threadManager, TfClientId clientId) override;
    STDMETHODIMP ActivateEx(ITfThreadMgr* threadManager, TfClientId clientId, DWORD flags) override;
    STDMETHODIMP Deactivate() override;
    STDMETHODIMP GetReloadState(DWORD* state) override;
    STDMETHODIMP RestoreReloadState(DWORD state) override;
    void retainEditSession() { ++pendingEditSessions_; AddRef(); }
    void releaseEditSession() { --pendingEditSessions_; Release(); }
    ReloadActivity deferReload() { return ReloadActivity(reloadActivity_); }

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL foreground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie editCookie, ITfComposition* composition) override;

    // ITfTextEditSink
    STDMETHODIMP OnEndEdit(ITfContext* context, TfEditCookie editCookie,
                           ITfEditRecord* editRecord) override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* documentManager) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* documentManager) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* focused, ITfDocumentMgr* previous) override;
    STDMETHODIMP OnPushContext(ITfContext* context) override;
    STDMETHODIMP OnPopContext(ITfContext* context) override;

    // ITfThreadFocusSink
    STDMETHODIMP OnSetThreadFocus() override;
    STDMETHODIMP OnKillThreadFocus() override;

    // ITfCompartmentEventSink
    STDMETHODIMP OnChange(REFGUID guid) override;

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** items) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid,
                                         ITfDisplayAttributeInfo** info) override;

    // ITfFunctionProvider
    STDMETHODIMP GetType(GUID* guid) override;
    STDMETHODIMP GetDescription(BSTR* description) override;
    STDMETHODIMP GetFunction(REFGUID guid, REFIID iid, IUnknown** object) override;

    // ITfFunction / ITfFnConfigure, which Windows calls for the IME's options button
    STDMETHODIMP GetDisplayName(BSTR* name) override;
    STDMETHODIMP Show(HWND parent, LANGID language, REFGUID profile) override;

    HRESULT processKey(TfEditCookie editCookie, ITfContext* context,
                       const KeyEvent& event, bool* handled);
    HRESULT commitCompositionForModeSwitch(TfEditCookie editCookie, ITfContext* context,
                                           bool moveCaret);
    void commitSessionDropped(ITfContext* context, unsigned generation);
    bool isChineseMode() const {
        return chineseMode_ && !(CurrentFrontendSettings().capsLockTogglesEnglish &&
                                  (GetKeyState(VK_CAPITAL) & 1));
    }
    bool isFullWidthMode() const noexcept { return fullWidthMode_; }
    void toggleChineseMode();
    void toggleFullWidthMode();
    bool toggleSimplifiedOutput();
    bool selectInputMethod(const std::string& identifier);
    // "/phrases" opens the phrase editor instead
    HRESULT openSettings(HWND parent = nullptr, const wchar_t* arguments = nullptr) const;
    bool isSymbolWindowVisible() const { return symbolWindow_.isVisible(); }
    void toggleSymbolWindow();
    HRESULT insertSymbol(TfEditCookie editCookie, ITfContext* context, const std::wstring& text);

private:
    friend struct TextServiceFocusTest;
    class KeyEditActivity;
    void finishKeyEdit();
    void reconcileDocumentFocus();
    void finishKeyPress(UINT virtualKey);
    unsigned keyEditDepth_ = 0;
    UINT keyDownVirtualKey_ = 0;
    bool deferredDocumentFocus_ = false;
    HRESULT adviseFunctionProvider();
    void unadviseFunctionProvider();
    ~TextService();

    bool isPotentialKey(const KeyEvent& event) const;
    bool isModeToggleKey(const KeyEvent& event) const;
    bool isWidthToggleKey(const KeyEvent& event) const;
    bool isShiftToggleKey(const KeyEvent& event) const;
    bool isFullWidthCharacterKey(const KeyEvent& event) const;
    HRESULT handleFrontendShortcut(ITfContext* context, const KeyEvent& event, BOOL* eaten);
    void sendSymbol(const std::wstring& text);
    // a synchronous request is refused when the document is locked or TSF will
    // not block the caller; the same session is then requested asynchronously
    HRESULT requestEditSession(ITfContext* context, ITfEditSession* session,
                               HRESULT* editResult, bool* retriedAsync = nullptr);
    HRESULT runKeySession(ITfContext* context, KeyEvent event, BOOL* eaten);
    HRESULT adviseInputModeSink();
    void unadviseInputModeSink();
    HRESULT adviseTextEditSink(ITfContext* context);
    void unadviseTextEditSink();
    HRESULT initializeLangBar();
    void uninitializeLangBar();
    void refreshLangBar();
    void notifyMode(const std::wstring& text);
    void setChineseMode(bool enabled);
    void setFullWidthMode(bool enabled);
    KeyEvent translateKey(WPARAM wparam, LPARAM lparam) const;
    HRESULT adviseSinks();
    void unadviseSinks();
    HRESULT updateComposition(TfEditCookie editCookie, ITfContext* context,
                              const EngineResult& result);
    HRESULT ensureComposition(TfEditCookie editCookie, ITfContext* context);
    HRESULT replaceCompositionText(TfEditCookie editCookie, ITfContext* context,
                                   const EngineResult& result);
    void applyDisplayAttributes(TfEditCookie editCookie, ITfContext* context,
                                ITfRange* range, const EngineResult& result);
    HRESULT commitText(TfEditCookie editCookie, ITfContext* context,
                       const std::wstring& text, bool filter = true);
    HRESULT endComposition(TfEditCookie editCookie, bool clearText);
    HRESULT convertCompositionForCommit(TfEditCookie editCookie);
    bool requestCommitComposition(bool moveCaret = true);
    void abandonComposition();
    void resetCandidateState();
    void updateCandidateWindow(TfEditCookie editCookie, ITfContext* context,
                               const EngineResult& result);
    bool selectionMatchesTrackedState(TfEditCookie editCookie, ITfContext* context) const;

    std::atomic<ULONG> referenceCount_{1};
    mutable unsigned reloadActivity_ = 0;
    unsigned pendingEditSessions_ = 0;
    ULONGLONG reloadNotBefore_ = 0;
    bool reloadKeyDownPending_ = false;
    bool reloadKeyUpPending_ = false;
    Microsoft::WRL::ComPtr<ITfThreadMgr> threadManager_;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    DWORD threadManagerCookie_ = TF_INVALID_COOKIE;
    DWORD threadFocusCookie_ = TF_INVALID_COOKIE;
    DWORD inputModeCookie_ = TF_INVALID_COOKIE;
    DWORD conversionModeCookie_ = TF_INVALID_COOKIE;
    DWORD textEditCookie_ = TF_INVALID_COOKIE;
    TfGuidAtom inputAttributeAtom_ = TF_INVALID_GUIDATOM;
    TfGuidAtom focusedAttributeAtom_ = TF_INVALID_GUIDATOM;
    bool chineseMode_ = true;
    bool secureMode_ = false;
    bool uiLessMode_ = false;
    Microsoft::WRL::ComPtr<ITfUIElementMgr> uiElementManager_;
    UIElementSession candidateUI_, notificationUI_, symbolUI_, punctuationUI_;
    void presentAuxiliaryUI(UIElementSession& session, int kind,
                            std::function<void(bool)> show, std::function<bool()> visible);
    void showSymbolWindow(bool userOpened = false);
    void showNotification(const std::wstring& text);
    bool keyboardAvailable(ITfContext* context) const;
    SharedCommitHistory commitHistory_;
    std::wstring historyHostPath_;
    bool historyHostReady_ = false;
    ULONGLONG nextHistoryHostAttempt_ = 0;
    std::wstring pendingCommitText_;
    void recordCommittedText(const std::wstring& text);
    bool fullWidthMode_ = false;
    bool shiftTogglePending_ = false;
    DWORD shiftPressedAt_ = 0;
    bool candidateActive_ = false;
    unsigned candidateGeneration_ = 0;
    bool endingComposition_ = false;
    bool pendingModeCommit_ = false;
    unsigned commitGeneration_ = 0;
    Microsoft::WRL::ComPtr<ITfComposition> composition_;
    Microsoft::WRL::ComPtr<ITfContext> compositionContext_;
    Microsoft::WRL::ComPtr<ITfContext> textEditContext_;
    Microsoft::WRL::ComPtr<ITfRange> candidateAnchor_;
    Microsoft::WRL::ComPtr<ITfContext> candidateContext_;
    std::unique_ptr<EngineSession> engine_;
    CandidateWindow candidateWindow_;
    NotificationWindow notificationWindow_;
    PunctuationKeyboard punctuationKeyboard_{[this](const std::wstring& text) { sendSymbol(text); }};
    SymbolWindow symbolWindow_{[this](const std::wstring& text) { sendSymbol(text); }};
    std::mutex langBarMutex_;
    LangBarButton* modeIconButton_ = nullptr;
    LangBarButton* switchLanguageButton_ = nullptr;
    LangBarButton* fullHalfButton_ = nullptr;
};

}  // namespace ChiaKey::WindowsTsf
