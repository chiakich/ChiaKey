#include "TextService.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <new>
#include <shellapi.h>
#include <utility>

#include "Diagnostics.h"
#include "EditSessionRequest.h"
#include "Guids.h"
#include "LangBarButton.h"
#include "ModuleState.h"
#include "OutputFilter.h"

namespace ChiaKey::WindowsTsf {
namespace {

using Microsoft::WRL::ComPtr;

constexpr DWORD kShiftTapTimeoutMilliseconds = 300;

class KeyEditSession final : public ITfEditSession {
public:
    KeyEditSession(TextService* service, ITfContext* context, KeyEvent event)
        : service_(service), context_(context), event_(std::move(event)) {
        service_->retainEditSession();
    }

    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfEditSession) {
            *object = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP DoEditSession(TfEditCookie editCookie) override {
        return service_->processKey(editCookie, context_.Get(), event_, &handled_);
    }

    bool handled() const { return handled_; }

private:
    ~KeyEditSession() { service_->releaseEditSession(); }
    std::atomic<ULONG> references_{1};
    TextService* service_;
    ComPtr<ITfContext> context_;
    KeyEvent event_;
    bool handled_ = false;
};

class TerminateEditSession final : public ITfEditSession {
public:
    TerminateEditSession(TextService* service, ITfComposition* composition)
        : service_(service), composition_(composition) { service_->retainEditSession(); }
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfEditSession) {
            *object = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP DoEditSession(TfEditCookie editCookie) override {
        if (!composition_) return S_OK;
        ComPtr<ITfRange> range;
        if (SUCCEEDED(composition_->GetRange(&range))) {
            range->SetText(editCookie, 0, nullptr, 0);
        }
        return composition_->EndComposition(editCookie);
    }

private:
    ~TerminateEditSession() { service_->releaseEditSession(); }
    std::atomic<ULONG> references_{1};
    TextService* service_;
    ComPtr<ITfComposition> composition_;
};

class CommitModeSwitchEditSession final : public ITfEditSession {
public:
    CommitModeSwitchEditSession(TextService* service, ITfContext* context, bool moveCaret,
                                unsigned generation)
        : service_(service), context_(context), moveCaret_(moveCaret), generation_(generation) {
        service_->retainEditSession();
    }
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfEditSession) {
            *object = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP DoEditSession(TfEditCookie editCookie) override {
        ran_ = true;
        return service_->commitCompositionForModeSwitch(editCookie, context_.Get(), moveCaret_);
    }

private:
    ~CommitModeSwitchEditSession() {
        // TSF drops a queued session whose context is destroyed before it gets the lock
        if (!ran_) service_->commitSessionDropped(context_.Get(), generation_);
        service_->releaseEditSession();
    }
    std::atomic<ULONG> references_{1};
    TextService* service_;
    ComPtr<ITfContext> context_;
    bool moveCaret_;
    unsigned generation_;
    bool ran_ = false;
};

class SymbolEditSession final : public ITfEditSession {
public:
    SymbolEditSession(TextService* service, ITfContext* context, std::wstring text)
        : service_(service), context_(context), text_(std::move(text)) {
        service_->retainEditSession();
    }
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfEditSession) {
            *object = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP DoEditSession(TfEditCookie editCookie) override {
        return service_->insertSymbol(editCookie, context_.Get(), text_);
    }

private:
    ~SymbolEditSession() { service_->releaseEditSession(); }
    std::atomic<ULONG> references_{1};
    TextService* service_;
    ComPtr<ITfContext> context_;
    std::wstring text_;
};

struct DisplayAttributeSpec {
    const GUID* guid;
    TF_DA_LINESTYLE lineStyle;
    const wchar_t* description;
};

constexpr DisplayAttributeSpec kDisplayAttributes[] = {
    {&kInputDisplayAttributeGuid, TF_LS_DOT, L"千秋輸入法組字"},
    {&kFocusedDisplayAttributeGuid, TF_LS_SOLID, L"千秋輸入法目前詞段"},
};

class DisplayAttributeInfo final : public ITfDisplayAttributeInfo {
public:
    explicit DisplayAttributeInfo(const DisplayAttributeSpec& spec) : spec_(spec) {
        ++g_objectCount;
    }
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfDisplayAttributeInfo) {
            *object = static_cast<ITfDisplayAttributeInfo*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP GetGUID(GUID* guid) override {
        if (!guid) return E_INVALIDARG;
        *guid = *spec_.guid;
        return S_OK;
    }
    STDMETHODIMP GetDescription(BSTR* description) override {
        if (!description) return E_INVALIDARG;
        *description = SysAllocString(spec_.description);
        return *description ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute) override {
        if (!attribute) return E_INVALIDARG;
        *attribute = {};
        attribute->crText.type = TF_CT_NONE;
        attribute->crBk.type = TF_CT_NONE;
        attribute->lsStyle = spec_.lineStyle;
        attribute->crLine.type = TF_CT_NONE;
        attribute->bAttr = TF_ATTR_INPUT;
        return S_OK;
    }
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override { return E_NOTIMPL; }
    STDMETHODIMP Reset() override { return S_OK; }

private:
    ~DisplayAttributeInfo() { --g_objectCount; }
    std::atomic<ULONG> references_{1};
    DisplayAttributeSpec spec_;
};

class DisplayAttributeEnum final : public IEnumTfDisplayAttributeInfo {
public:
    explicit DisplayAttributeEnum(size_t next = 0) : next_(next) { ++g_objectCount; }
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_IEnumTfDisplayAttributeInfo) {
            *object = static_cast<IEnumTfDisplayAttributeInfo*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** result) override {
        if (!result) return E_INVALIDARG;
        *result = new (std::nothrow) DisplayAttributeEnum(next_);
        return *result ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP Next(ULONG count, ITfDisplayAttributeInfo** items, ULONG* fetched) override {
        if (!items || (!fetched && count != 1)) return E_INVALIDARG;
        ULONG produced = 0;
        while (produced < count && next_ < std::size(kDisplayAttributes)) {
            items[produced] = new (std::nothrow) DisplayAttributeInfo(kDisplayAttributes[next_]);
            if (!items[produced]) break;
            ++produced;
            ++next_;
        }
        if (fetched) *fetched = produced;
        return produced == count ? S_OK : S_FALSE;
    }
    STDMETHODIMP Reset() override {
        next_ = 0;
        return S_OK;
    }
    STDMETHODIMP Skip(ULONG count) override {
        const size_t remaining = std::size(kDisplayAttributes) - next_;
        next_ += std::min<size_t>(count, remaining);
        return count <= remaining ? S_OK : S_FALSE;
    }

private:
    ~DisplayAttributeEnum() { --g_objectCount; }
    std::atomic<ULONG> references_{1};
    size_t next_ = 0;
};

// bpmf-punctuations.cin's _ctrl_opt_ chords; Ctrl+Alt+. is the symbol window's instead
constexpr UINT kPunctuationChordKeys[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', VK_OEM_1, VK_OEM_7, VK_OEM_COMMA, VK_OEM_2,
};
constexpr wchar_t kPunctuationChordName[] = L"千秋輸入法標點";
constexpr TF_PRESERVEDKEY kSymbolWindowKey{VK_OEM_PERIOD, TF_MOD_CONTROL | TF_MOD_ALT};

GUID PunctuationChordGuid(size_t index) {
    GUID guid = kPunctuationChordKeyGuidBase;
    guid.Data4[7] = static_cast<unsigned char>(index);
    return guid;
}

bool IsKeyDown(UINT virtualKey) {
    return (GetKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
}

bool IsShiftKey(UINT virtualKey) {
    return virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT;
}

bool IsHostEditingKey(UINT virtualKey) {
    switch (virtualKey) {
        case VK_BACK:
        case VK_DELETE:
        case VK_RETURN:
        case VK_SPACE:
        case VK_TAB:
        case VK_ESCAPE:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
            return true;
        default:
            return false;
    }
}

wchar_t PrintableCharacter(const KeyEvent& event) {
    if (event.text.size() == 1 && event.text.front() >= L' ' && event.text.front() <= L'~') {
        return event.text.front();
    }
    const ChiaKey::KeyEvent key = MakeCoreKey(event);
    return key.receivedString.size() == 1 && key.receivedString[0] >= ' ' &&
                   key.receivedString[0] <= '~'
               ? static_cast<wchar_t>(key.receivedString[0])
               : 0;
}

HRESULT MoveCaret(TfEditCookie editCookie, ITfContext* context, ITfRange* range) {
    TF_SELECTION selection{};
    selection.range = range;
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    return context->SetSelection(editCookie, 1, &selection);
}

}  // namespace

// Hosts can pump TSF focus callbacks while a key edit is writing the first
// preedit. Excel briefly clears focus here before restoring the same context.
// Cover both the request (synchronous callbacks) and processKey (queued edits).
class TextService::KeyEditActivity final {
public:
    explicit KeyEditActivity(TextService& service) : service_(service) {
        ++service_.keyEditDepth_;
    }
    ~KeyEditActivity() { service_.finishKeyEdit(); }
    KeyEditActivity(const KeyEditActivity&) = delete;
    KeyEditActivity& operator=(const KeyEditActivity&) = delete;
private:
    TextService& service_;
};

void TextService::finishKeyEdit() {
    if (--keyEditDepth_ == 0) reconcileDocumentFocus();
}

void TextService::finishKeyPress(UINT virtualKey) {
    if (keyDownVirtualKey_ != virtualKey) return;
    keyDownVirtualKey_ = 0;
    reconcileDocumentFocus();
}

void TextService::reconcileDocumentFocus() {
    if (keyEditDepth_ || keyDownVirtualKey_ || !deferredDocumentFocus_) return;
    deferredDocumentFocus_ = false;
    ComPtr<ITfDocumentMgr> focused;
    if (threadManager_ && SUCCEEDED(threadManager_->GetFocus(&focused))) {
        // Query the final focus instead of replaying transient notifications.
        // A genuine switch must still commit and reconnect the text-edit sink.
        OnSetFocus(focused.Get(), nullptr);
    }
}

TextService::TextService() { ++g_objectCount; }
TextService::~TextService() {
    candidateUI_.end(); notificationUI_.end(); symbolUI_.end(); punctuationUI_.end();
    --g_objectCount;
}

HRESULT TextService::CreateInstance(IUnknown* outer, REFIID iid, void** object) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (outer) return CLASS_E_NOAGGREGATION;
    auto* service = new (std::nothrow) TextService();
    if (!service) return E_OUTOFMEMORY;
    const HRESULT result = service->QueryInterface(iid, object);
    service->Release();
    return result;
}

STDMETHODIMP TextService::QueryInterface(REFIID iid, void** object) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (iid == IID_IUnknown || iid == IID_ITfTextInputProcessor ||
        iid == IID_ITfTextInputProcessorEx) {
        *object = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (iid == IID_ITfKeyEventSink) {
        *object = static_cast<ITfKeyEventSink*>(this);
    } else if (iid == IID_ITfCompositionSink) {
        *object = static_cast<ITfCompositionSink*>(this);
    } else if (iid == IID_ITfTextEditSink) {
        *object = static_cast<ITfTextEditSink*>(this);
    } else if (iid == IID_ITfThreadMgrEventSink) {
        *object = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (iid == IID_ITfThreadFocusSink) {
        *object = static_cast<ITfThreadFocusSink*>(this);
    } else if (iid == IID_ITfCompartmentEventSink) {
        *object = static_cast<ITfCompartmentEventSink*>(this);
    } else if (iid == IID_ITfDisplayAttributeProvider) {
        *object = static_cast<ITfDisplayAttributeProvider*>(this);
    } else if (iid == IID_ITfFunctionProvider) {
        *object = static_cast<ITfFunctionProvider*>(this);
    } else if (iid == IID_ITfFnConfigure || iid == IID_ITfFunction) {
        *object = static_cast<ITfFnConfigure*>(this);
    } else if (iid == __uuidof(IChiaKeyReloadControl)) {
        *object = static_cast<IChiaKeyReloadControl*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) TextService::AddRef() { return ++referenceCount_; }
STDMETHODIMP_(ULONG) TextService::Release() {
    const ULONG remaining = --referenceCount_;
    if (!remaining) delete this;
    return remaining;
}

STDMETHODIMP TextService::GetReloadState(DWORD* state) {
    if (!state) return E_INVALIDARG;
    *state = (chineseMode_ ? kReloadChinese : 0) | (fullWidthMode_ ? kReloadFullWidth : 0);
    if (reloadActivity_ || pendingEditSessions_ || reloadKeyDownPending_ || reloadKeyUpPending_ ||
        composition_ || pendingModeCommit_ ||
        endingComposition_ || candidateActive_ || shiftTogglePending_ ||
        !pendingCommitText_.empty() || (engine_ && engine_->hasComposition()) ||
        symbolWindow_.isVisible() || punctuationKeyboard_.isVisible() ||
        GetTickCount64() < reloadNotBefore_) return S_FALSE;
    GUITHREADINFO gui{sizeof(gui)};
    if (GetGUIThreadInfo(GetCurrentThreadId(), &gui) &&
        (gui.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) return S_FALSE;
    // Do not split a physical key press across versions (including modifiers).
    for (int key = 1; key < 256; ++key)
        if (GetAsyncKeyState(key) & 0x8000) return S_FALSE;
    return S_OK;
}

STDMETHODIMP TextService::RestoreReloadState(DWORD state) {
    ReloadActivity activity(reloadActivity_);
    if (!engine_ || !engine_->ready()) return E_FAIL;
    setChineseMode((state & kReloadChinese) != 0);
    setFullWidthMode((state & kReloadFullWidth) != 0);
    return S_OK;
}

STDMETHODIMP TextService::Activate(ITfThreadMgr* threadManager, TfClientId clientId) {
    ReloadActivity activity(reloadActivity_);
    return ActivateEx(threadManager, clientId, 0);
}

STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* threadManager, TfClientId clientId,
                                     DWORD flags) {
    ReloadActivity activity(reloadActivity_);
    if (!threadManager || clientId == TF_CLIENTID_NULL) return E_INVALIDARG;
    if (threadManager_) return S_OK;

    secureMode_ = (flags & TF_TMAE_SECUREMODE) != 0;
    uiLessMode_ = (flags & TF_TMAE_UIELEMENTENABLEDONLY) != 0;
    threadManager->QueryInterface(IID_PPV_ARGS(&uiElementManager_));
    Trace("Activate flags=0x%08lX uiLess=%d uiManager=%d", flags, uiLessMode_, uiElementManager_ != nullptr);
    threadManager_ = threadManager;
    clientId_ = clientId;
    ComPtr<ITfCategoryMgr> categoryManager;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&categoryManager)))) {
        categoryManager->RegisterGUID(kInputDisplayAttributeGuid, &inputAttributeAtom_);
        categoryManager->RegisterGUID(kFocusedDisplayAttributeGuid, &focusedAttributeAtom_);
    }
    engine_ = EngineSession::Create();
    const auto settingsApp = SettingsAppPath();
    const auto separator = settingsApp.find_last_of(L"\\/");
    historyHostPath_ = separator == std::wstring::npos ? std::wstring()
        : settingsApp.substr(0, separator) + L"\\ChiaKeyStateHost.exe";
    historyHostReady_ = false;
    nextHistoryHostAttempt_ = 0;
    Trace("Activate engineReady=%d", engine_ && engine_->ready());
    const HRESULT langBarResult = initializeLangBar();
    Trace("InitializeLangBar hr=0x%08lX", static_cast<unsigned long>(langBarResult));
    HRESULT result = adviseSinks();
    if (SUCCEEDED(result)) {
        ComPtr<ITfDocumentMgr> focused;
        ComPtr<ITfContext> context;
        if (SUCCEEDED(threadManager_->GetFocus(&focused)) && focused &&
            SUCCEEDED(focused->GetTop(&context)) && context) {
            adviseTextEditSink(context.Get());
        }
        const HRESULT providerResult = adviseFunctionProvider();
        if (FAILED(providerResult)) {
            Trace("AdviseFunctionProvider hr=0x%08lX", static_cast<unsigned long>(providerResult));
        }
        setChineseMode(true);
        setFullWidthMode(false);
        BOOL threadFocused = FALSE;
        if (SUCCEEDED(threadManager_->IsThreadFocus(&threadFocused)) && threadFocused &&
            ReadSymbolWindowState().visible) {
            showSymbolWindow();
        }
    } else {
        Trace("AdviseSinks hr=0x%08lX", static_cast<unsigned long>(result));
        unadviseSinks();
        uninitializeLangBar();
        engine_.reset();
        uiElementManager_.Reset();
        threadManager_.Reset();
        clientId_ = TF_CLIENTID_NULL;
    }
    return result;
}

STDMETHODIMP TextService::Deactivate() {
    ReloadActivity activity(reloadActivity_);
    keyDownVirtualKey_ = 0;
    deferredDocumentFocus_ = false;
    reloadKeyDownPending_ = reloadKeyUpPending_ = false;
    if (!requestCommitComposition()) {
        Trace("Deactivate: composition could not be committed");
    }
    unadviseFunctionProvider();
    unadviseSinks();
    uninitializeLangBar();
    resetCandidateState();
    notificationUI_.end(); symbolUI_.end(); punctuationUI_.end();
    uiElementManager_.Reset();
    // another input method takes over; the saved state brings the window back with ChiaKey
    symbolWindow_.destroy();
    punctuationKeyboard_.destroy();
    notificationWindow_.destroy();
    // the context saves its learning when it ends; an app closing leaves no other chance
    engine_.reset();
    threadManager_.Reset();
    clientId_ = TF_CLIENTID_NULL;
    inputAttributeAtom_ = TF_INVALID_GUIDATOM;
    focusedAttributeAtom_ = TF_INVALID_GUIDATOM;
    return S_OK;
}

HRESULT TextService::adviseSinks() {
    ReloadActivity activity(reloadActivity_);
    ComPtr<ITfKeystrokeMgr> keystrokes;
    HRESULT result = threadManager_.As(&keystrokes);
    if (FAILED(result)) return result;
    result = keystrokes->AdviseKeyEventSink(clientId_, this, TRUE);
    if (FAILED(result)) return result;
    // TSF never passes Ctrl+Alt chords to the key sink, so they are preserved keys;
    // the symbol window takes over the lexicon's Ctrl+Alt+. for ．
    static constexpr wchar_t kSymbolKeyName[] = L"符號表";
    HRESULT preserveResult =
        keystrokes->PreserveKey(clientId_, kSymbolWindowKeyGuid, &kSymbolWindowKey,
                                kSymbolKeyName, static_cast<ULONG>(std::size(kSymbolKeyName) - 1));
    for (size_t index = 0; index < std::size(kPunctuationChordKeys); ++index) {
        const TF_PRESERVEDKEY chord{kPunctuationChordKeys[index], TF_MOD_CONTROL | TF_MOD_ALT};
        const HRESULT chordResult = keystrokes->PreserveKey(
            clientId_, PunctuationChordGuid(index), &chord, kPunctuationChordName,
            static_cast<ULONG>(std::size(kPunctuationChordName) - 1));
        if (FAILED(chordResult)) preserveResult = chordResult;
    }
    if (FAILED(preserveResult)) {
        Trace("PreserveKey hr=0x%08lX", static_cast<unsigned long>(preserveResult));
    }

    ComPtr<ITfSource> source;
    result = threadManager_.As(&source);
    if (FAILED(result)) {
        keystrokes->UnadviseKeyEventSink(clientId_);
        return result;
    }
    result = source->AdviseSink(IID_ITfThreadMgrEventSink,
                                static_cast<ITfThreadMgrEventSink*>(this),
                                &threadManagerCookie_);
    if (FAILED(result)) {
        keystrokes->UnadviseKeyEventSink(clientId_);
        return result;
    }
    const HRESULT focusResult = source->AdviseSink(
        IID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink*>(this), &threadFocusCookie_);
    if (FAILED(focusResult)) {
        Trace("AdviseThreadFocus hr=0x%08lX", static_cast<unsigned long>(focusResult));
    }
    const HRESULT modeResult = adviseInputModeSink();
    if (FAILED(modeResult)) {
        Trace("AdviseInputMode hr=0x%08lX", static_cast<unsigned long>(modeResult));
    }
    return S_OK;
}

void TextService::unadviseSinks() {
    ReloadActivity activity(reloadActivity_);
    if (!threadManager_) return;
    unadviseTextEditSink();
    unadviseInputModeSink();
    ComPtr<ITfKeystrokeMgr> keystrokes;
    if (SUCCEEDED(threadManager_.As(&keystrokes)) && clientId_ != TF_CLIENTID_NULL) {
        keystrokes->UnpreserveKey(kSymbolWindowKeyGuid, &kSymbolWindowKey);
        for (size_t index = 0; index < std::size(kPunctuationChordKeys); ++index) {
            const TF_PRESERVEDKEY chord{kPunctuationChordKeys[index],
                                        TF_MOD_CONTROL | TF_MOD_ALT};
            keystrokes->UnpreserveKey(PunctuationChordGuid(index), &chord);
        }
        keystrokes->UnadviseKeyEventSink(clientId_);
    }
    ComPtr<ITfSource> source;
    threadManager_.As(&source);
    for (DWORD* cookie : {&threadManagerCookie_, &threadFocusCookie_}) {
        if (*cookie != TF_INVALID_COOKIE && source) source->UnadviseSink(*cookie);
        *cookie = TF_INVALID_COOKIE;
    }
}

HRESULT TextService::adviseTextEditSink(ITfContext* context) {
    ReloadActivity activity(reloadActivity_);
    if (textEditContext_.Get() == context && textEditCookie_ != TF_INVALID_COOKIE) {
        return S_OK;
    }
    unadviseTextEditSink();
    if (!context) return S_OK;

    ComPtr<ITfSource> source;
    HRESULT result = context->QueryInterface(IID_PPV_ARGS(&source));
    if (FAILED(result)) return result;
    result = source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this),
                                &textEditCookie_);
    if (SUCCEEDED(result)) textEditContext_ = context;
    return result;
}

void TextService::unadviseTextEditSink() {
    ReloadActivity activity(reloadActivity_);
    if (textEditContext_ && textEditCookie_ != TF_INVALID_COOKIE) {
        ComPtr<ITfSource> source;
        if (SUCCEEDED(textEditContext_.As(&source))) source->UnadviseSink(textEditCookie_);
    }
    textEditCookie_ = TF_INVALID_COOKIE;
    textEditContext_.Reset();
}

HRESULT TextService::adviseInputModeSink() {
    ReloadActivity activity(reloadActivity_);
    ComPtr<ITfCompartmentMgr> manager;
    HRESULT result = threadManager_.As(&manager);
    if (FAILED(result)) return result;
    ComPtr<ITfCompartment> compartment;
    result = manager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &compartment);
    if (FAILED(result)) return result;
    ComPtr<ITfSource> source;
    result = compartment.As(&source);
    if (FAILED(result)) return result;
    result = source->AdviseSink(IID_ITfCompartmentEventSink,
                                static_cast<ITfCompartmentEventSink*>(this),
                                &inputModeCookie_);
    if (FAILED(result)) return result;

    ComPtr<ITfCompartment> conversion;
    ComPtr<ITfSource> conversionSource;
    if (SUCCEEDED(manager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                                          &conversion)) &&
        SUCCEEDED(conversion.As(&conversionSource))) {
        conversionSource->AdviseSink(IID_ITfCompartmentEventSink,
                                     static_cast<ITfCompartmentEventSink*>(this),
                                     &conversionModeCookie_);
    }
    return S_OK;
}

void TextService::unadviseInputModeSink() {
    ReloadActivity activity(reloadActivity_);
    if (!threadManager_) return;
    ComPtr<ITfCompartmentMgr> manager;
    ComPtr<ITfCompartment> compartment;
    ComPtr<ITfSource> source;
    if (inputModeCookie_ != TF_INVALID_COOKIE && SUCCEEDED(threadManager_.As(&manager)) &&
        SUCCEEDED(manager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                          &compartment)) &&
        SUCCEEDED(compartment.As(&source))) {
        source->UnadviseSink(inputModeCookie_);
    }
    inputModeCookie_ = TF_INVALID_COOKIE;

    ComPtr<ITfCompartment> conversion;
    ComPtr<ITfSource> conversionSource;
    if (conversionModeCookie_ != TF_INVALID_COOKIE && manager &&
        SUCCEEDED(manager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                                          &conversion)) &&
        SUCCEEDED(conversion.As(&conversionSource))) {
        conversionSource->UnadviseSink(conversionModeCookie_);
    }
    conversionModeCookie_ = TF_INVALID_COOKIE;
}

HRESULT TextService::adviseFunctionProvider() {
    ReloadActivity activity(reloadActivity_);
    ComPtr<ITfSourceSingle> source;
    HRESULT result = threadManager_.As(&source);
    if (FAILED(result)) return result;
    return source->AdviseSingleSink(clientId_, IID_ITfFunctionProvider,
                                    static_cast<ITfFunctionProvider*>(this));
}

void TextService::unadviseFunctionProvider() {
    ReloadActivity activity(reloadActivity_);
    if (!threadManager_ || clientId_ == TF_CLIENTID_NULL) return;
    ComPtr<ITfSourceSingle> source;
    if (SUCCEEDED(threadManager_.As(&source))) {
        source->UnadviseSingleSink(clientId_, IID_ITfFunctionProvider);
    }
}

HRESULT TextService::openSettings(HWND parent, const wchar_t* arguments) const {
    ReloadActivity activity(reloadActivity_);
    const HINSTANCE launched = ShellExecuteW(parent, L"open", SettingsAppPath().c_str(), arguments,
                                             nullptr, SW_SHOWNORMAL);
    const INT_PTR code = reinterpret_cast<INT_PTR>(launched);
    return code > 32 ? S_OK : HRESULT_FROM_WIN32(static_cast<DWORD>(code));
}

STDMETHODIMP TextService::GetType(GUID* guid) {
    ReloadActivity activity(reloadActivity_);
    if (!guid) return E_INVALIDARG;
    *guid = kTextServiceClsid;
    return S_OK;
}

STDMETHODIMP TextService::GetDescription(BSTR* description) {
    ReloadActivity activity(reloadActivity_);
    if (!description) return E_INVALIDARG;
    *description = SysAllocString(kTextServiceDescription);
    return *description ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::GetFunction(REFGUID guid, REFIID iid, IUnknown** object) {
    ReloadActivity activity(reloadActivity_);
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (guid != GUID_NULL || iid != IID_ITfFnConfigure) return E_NOINTERFACE;
    return QueryInterface(iid, reinterpret_cast<void**>(object));
}

STDMETHODIMP TextService::GetDisplayName(BSTR* name) {
    ReloadActivity activity(reloadActivity_);
    if (!name) return E_INVALIDARG;
    *name = SysAllocString(L"千秋輸入法設定");
    return *name ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::Show(HWND parent, LANGID, REFGUID) {
    ReloadActivity activity(reloadActivity_);
    return openSettings(parent);
}

HRESULT TextService::initializeLangBar() {
    ReloadActivity activity(reloadActivity_);
    if (!threadManager_) return E_UNEXPECTED;
    ComPtr<ITfLangBarItemMgr> manager;
    HRESULT result = threadManager_.As(&manager);
    if (FAILED(result)) return result;

    auto* modeIcon = new (std::nothrow)
        LangBarButton(this, kLangBarInputModeGuid, LangBarButton::Kind::InputMode);
    auto* switchLanguage = new (std::nothrow) LangBarButton(
        this, kLangBarSwitchLanguageGuid, LangBarButton::Kind::SwitchLanguage);
    auto* fullHalf = new (std::nothrow)
        LangBarButton(this, kLangBarFullHalfGuid, LangBarButton::Kind::FullHalf);
    {
        std::lock_guard<std::mutex> lock(langBarMutex_);
        modeIconButton_ = modeIcon;
        switchLanguageButton_ = switchLanguage;
        fullHalfButton_ = fullHalf;
    }
    if (!modeIcon || !switchLanguage || !fullHalf) {
        uninitializeLangBar();
        return E_OUTOFMEMORY;
    }

    LangBarButton* buttons[] = {modeIcon, switchLanguage, fullHalf};
    for (LangBarButton* button : buttons) {
        result = button->startThemeTracking();
        if (SUCCEEDED(result)) result = manager->AddItem(button);
        if (FAILED(result)) {
            uninitializeLangBar();
            return result;
        }
    }
    return S_OK;
}

void TextService::uninitializeLangBar() {
    ReloadActivity activity(reloadActivity_);
    ComPtr<ITfLangBarItemMgr> manager;
    if (threadManager_) threadManager_.As(&manager);
    std::array<LangBarButton*, 3> buttons{};
    {
        std::lock_guard<std::mutex> lock(langBarMutex_);
        buttons = {modeIconButton_, switchLanguageButton_, fullHalfButton_};
        modeIconButton_ = nullptr;
        switchLanguageButton_ = nullptr;
        fullHalfButton_ = nullptr;
    }
    for (LangBarButton* button : buttons) {
        if (!button) continue;
        button->stopThemeTracking();
        if (manager) manager->RemoveItem(button);
        button->Release();
    }
}

void TextService::refreshLangBar() {
    ReloadActivity activity(reloadActivity_);
    std::array<LangBarButton*, 3> buttons{};
    {
        std::lock_guard<std::mutex> lock(langBarMutex_);
        buttons = {modeIconButton_, switchLanguageButton_, fullHalfButton_};
        for (LangBarButton* button : buttons) {
            if (button) button->AddRef();
        }
    }
    for (LangBarButton* button : buttons) {
        if (!button) continue;
        button->update();
        button->Release();
    }
}

void TextService::notifyMode(const std::wstring& text) {
    ReloadActivity activity(reloadActivity_);
    if (secureMode_ || !CurrentFrontendSettings().showNotifications) return;
    BOOL focused = FALSE;
    if (threadManager_ && SUCCEEDED(threadManager_->IsThreadFocus(&focused)) && focused)
        showNotification(text);
}

void TextService::setChineseMode(bool enabled) {
    ReloadActivity activity(reloadActivity_);
    // ending a composition without clearing its range commits the visible text
    if (!enabled && !requestCommitComposition()) {
        Trace("InputMode switch deferred: composition commit unavailable");
        setChineseMode(true);
        return;
    }
    const bool changed = chineseMode_ != enabled;
    chineseMode_ = enabled;
    shiftTogglePending_ = false;
    shiftPressedAt_ = 0;

    ComPtr<ITfCompartmentMgr> manager;
    ComPtr<ITfCompartment> compartment;
    if (threadManager_ && SUCCEEDED(threadManager_.As(&manager)) &&
        SUCCEEDED(manager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                          &compartment))) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        value.lVal = enabled ? 1 : 0;
        compartment->SetValue(clientId_, &value);
    }
    refreshLangBar();
    if (changed) notifyMode(UiText(enabled ? L"中文模式" : L"英文模式"));
}

void TextService::toggleChineseMode() {
    ReloadActivity activity(reloadActivity_);
    setChineseMode(!chineseMode_);
}

void TextService::setFullWidthMode(bool enabled) {
    ReloadActivity activity(reloadActivity_);
    const bool changed = fullWidthMode_ != enabled;
    fullWidthMode_ = enabled;
    ComPtr<ITfCompartmentMgr> manager;
    ComPtr<ITfCompartment> compartment;
    if (threadManager_ && SUCCEEDED(threadManager_.As(&manager)) &&
        SUCCEEDED(manager->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                                          &compartment))) {
        LONG mode = 0;
        VARIANT current;
        VariantInit(&current);
        if (SUCCEEDED(compartment->GetValue(&current)) && current.vt == VT_I4) {
            mode = current.lVal;
        }
        VariantClear(&current);
        if (enabled) {
            mode |= TF_CONVERSIONMODE_FULLSHAPE;
        } else {
            mode &= ~TF_CONVERSIONMODE_FULLSHAPE;
        }
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        value.lVal = mode;
        compartment->SetValue(clientId_, &value);
    }
    refreshLangBar();
    if (changed) notifyMode(UiText(enabled ? L"全形英數模式" : L"半形英數模式"));
}

void TextService::toggleFullWidthMode() {
    ReloadActivity activity(reloadActivity_);
    setFullWidthMode(!fullWidthMode_);
}

bool TextService::toggleSimplifiedOutput() {
    ReloadActivity activity(reloadActivity_);
    RefreshSettings();
    const bool changed = SetSimplifiedOutput(!CurrentFrontendSettings().simplifiedOutput);
    if (changed) {
        refreshLangBar();
        notifyMode(UiText(CurrentFrontendSettings().simplifiedOutput ? L"簡體中文輸出" : L"繁體中文輸出"));
    }
    return changed;
}

bool TextService::selectInputMethod(const std::string& identifier) {
    ReloadActivity activity(reloadActivity_);
    const bool changed = CurrentInputMethod() != identifier;
    if (changed) {
        // the switch rebuilds every context, which would drop the composition
        if (!requestCommitComposition()) return false;
        if (!SelectInputMethod(identifier)) return false;
    }
    if (!chineseMode_) setChineseMode(true);
    refreshLangBar();
    if (changed) notifyMode(UiText(L"選用") + InputMethodName(identifier) + UiText(L"輸入法"));
    return true;
}

KeyEvent TextService::translateKey(WPARAM wparam, LPARAM lparam) const {
    ReloadActivity activity(reloadActivity_);
    KeyEvent event;
    event.virtualKey = static_cast<UINT>(wparam);
    event.shift = IsKeyDown(VK_SHIFT);
    event.control = IsKeyDown(VK_CONTROL);
    event.alt = IsKeyDown(VK_MENU);
    event.capsLock = (GetKeyState(VK_CAPITAL) & 1) != 0;
    event.numLock = (GetKeyState(VK_NUMLOCK) & 1) != 0;

    BYTE keyboardState[256]{};
    if (!GetKeyboardState(keyboardState)) return event;
    wchar_t characters[8]{};
    const UINT scanCode = static_cast<UINT>((lparam >> 16) & 0xff);
    // flag 4 keeps ToUnicodeEx from changing the dead-key state of the host
    const int length = ToUnicodeEx(event.virtualKey, scanCode, keyboardState, characters,
                                   static_cast<int>(std::size(characters)), 4,
                                   GetKeyboardLayout(0));
    if (length > 0) event.text.assign(characters, characters + length);
    return event;
}

bool TextService::isPotentialKey(const KeyEvent& event) const {
    ReloadActivity activity(reloadActivity_);
    if (CapsLockAlphanumeric(event, CurrentFrontendSettings()) &&
        AlphanumericCharacter(event, CurrentFrontendSettings())) return true;
    if (isFullWidthCharacterKey(event)) return true;
    if (!isChineseMode() || !engine_ || !engine_->ready()) return false;
    // some hosts lose a key once it is claimed here, even if OnKeyDown declines
    if (!composition_ && !candidateActive_ && IsHostEditingKey(event.virtualKey)) {
        return false;
    }
    return engine_->wantsKey(event);
}

bool TextService::isModeToggleKey(const KeyEvent& event) const {
    ReloadActivity activity(reloadActivity_);
    if (!event.control || event.alt) return false;
    return event.virtualKey == VK_SPACE;
}

bool TextService::isShiftToggleKey(const KeyEvent& event) const {
    ReloadActivity activity(reloadActivity_);
    return IsShiftKey(event.virtualKey) && !event.control && !event.alt &&
           CurrentFrontendSettings().shiftTogglesEnglish &&
           !CurrentFrontendSettings().capsLockTogglesEnglish;
}

bool TextService::isWidthToggleKey(const KeyEvent& event) const {
    ReloadActivity activity(reloadActivity_);
    return event.virtualKey == VK_SPACE && event.shift && !event.control && !event.alt;
}

bool TextService::isFullWidthCharacterKey(const KeyEvent& event) const {
    ReloadActivity activity(reloadActivity_);
    return fullWidthMode_ && !event.control && !event.alt && PrintableCharacter(event) != 0;
}

void TextService::toggleSymbolWindow() {
    ReloadActivity activity(reloadActivity_);
    if (symbolWindow_.isVisible()) {
        symbolWindow_.close();
        symbolUI_.end();
    } else {
        showSymbolWindow(true);
    }
    refreshLangBar();
}

void TextService::sendSymbol(const std::wstring& text) {
    ReloadActivity activity(reloadActivity_);
    ComPtr<ITfDocumentMgr> focused;
    ComPtr<ITfContext> context;
    if (!threadManager_ || clientId_ == TF_CLIENTID_NULL ||
        FAILED(threadManager_->GetFocus(&focused)) || !focused ||
        FAILED(focused->GetTop(&context)) || !context) {
        Trace("Symbol: no focused context");
        MessageBeep(MB_OK);
        return;
    }
    auto* session = new (std::nothrow) SymbolEditSession(this, context.Get(), text);
    if (!session) return;
    HRESULT editResult = E_FAIL;
    HRESULT requestResult = requestEditSession(context.Get(), session, &editResult);
    if (FAILED(requestResult) || FAILED(editResult)) {
        Trace("Symbol request=0x%08lX edit=0x%08lX", static_cast<unsigned long>(requestResult),
              static_cast<unsigned long>(editResult));
    }
    session->Release();
}

HRESULT TextService::insertSymbol(TfEditCookie editCookie, ITfContext* context,
                                  const std::wstring& text) {
    ReloadActivity activity(reloadActivity_);
    reloadNotBefore_ = GetTickCount64() + 500;
    // the sentence being composed goes first, as typing a punctuation key would do
    if (composition_ && compositionContext_.Get() == context) {
        const HRESULT result = commitCompositionForModeSwitch(editCookie, context, true);
        if (FAILED(result)) return result;
    } else if (composition_) {
        abandonComposition();
    } else {
        resetCandidateState();
        if (engine_) engine_->reset();
    }
    return commitText(editCookie, context, text);
}

STDMETHODIMP TextService::OnSetFocus(BOOL foreground) {
    ReloadActivity activity(reloadActivity_);
    if (!foreground) {
        if (GetWindowThreadProcessId(GetForegroundWindow(), nullptr) != GetCurrentThreadId())
            punctuationKeyboard_.close();
        shiftTogglePending_ = false;
        if (!requestCommitComposition()) {
            Trace("Input focus lost: composition could not be committed");
        }
    }
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam,
                                        BOOL* eaten) {
    ReloadActivity activity(reloadActivity_);
    ReloadKeyTest tested(reloadKeyDownPending_, eaten);
    reloadNotBefore_ = GetTickCount64() + 500;
    if (!eaten) return E_INVALIDARG;
    *eaten = FALSE;
    if (!keyboardAvailable(context)) {
        shiftTogglePending_ = false;
        shiftPressedAt_ = 0;
        return S_OK;
    }
    if (!engine_ || !engine_->hasComposition()) RefreshSettings(true);
    const KeyEvent event = translateKey(wparam, lparam);
    if (!IsShiftKey(event.virtualKey)) {
        shiftTogglePending_ = false;
        shiftPressedAt_ = 0;
    }
    if (isShiftToggleKey(event)) {
        // OnKeyDown only runs for keys claimed here; it leaves Shift uneaten
        *eaten = TRUE;
        return S_OK;
    }
    *eaten = (punctuationKeyboard_.isVisible() && !PunctuationModifier(event.virtualKey)) ||
             isModeToggleKey(event) || isWidthToggleKey(event) || isPotentialKey(event) ||
             ShortcutFor(event, CurrentFrontendSettings()) != FrontendShortcut::None;
    if (event.virtualKey == VK_SPACE || event.virtualKey == VK_TAB) {
        Trace("TestKey vk=%u ctrl=%d shift=%d alt=%d chinese=%d full=%d composition=%d engineComposition=%d candidates=%d eaten=%d",
              event.virtualKey, event.control, event.shift, event.alt, chineseMode_, fullWidthMode_,
              composition_.Get() != nullptr, engine_ && engine_->hasComposition(), candidateActive_, *eaten);
    }
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext* context, WPARAM wparam, LPARAM, BOOL* eaten) {
    ReloadActivity activity(reloadActivity_);
    // TSF does not call OnKeyUp for unclaimed releases, but still tests them.
    finishKeyPress(static_cast<UINT>(wparam));
    ReloadKeyTest tested(reloadKeyUpPending_, eaten);
    reloadNotBefore_ = GetTickCount64() + 500;
    if (!eaten) return E_INVALIDARG;
    if (IsShiftKey(static_cast<UINT>(wparam)) && keyboardAvailable(context) &&
        engine_ && engine_->wantsShiftRelease()) {
        shiftTogglePending_ = false;
        *eaten = TRUE;
        return S_OK;
    }
    if (wparam == VK_CAPITAL && CurrentFrontendSettings().capsLockTogglesEnglish) {
        *eaten = TRUE; // Observe the new latch state, then pass the key up to Windows.
        return S_OK;
    }
    *eaten = IsShiftKey(static_cast<UINT>(wparam)) && shiftTogglePending_ &&
             !IsKeyDown(VK_CONTROL) && !IsKeyDown(VK_MENU) &&
             GetTickCount() - shiftPressedAt_ <= kShiftTapTimeoutMilliseconds;
    if (IsShiftKey(static_cast<UINT>(wparam)) && !*eaten) {
        shiftTogglePending_ = false;
        shiftPressedAt_ = 0;
    }
    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam,
                                    BOOL* eaten) {
    ReloadActivity activity(reloadActivity_);
    reloadKeyDownPending_ = false;
    reloadNotBefore_ = GetTickCount64() + 500;
    if (!context || !eaten) return E_INVALIDARG;
    *eaten = FALSE;
    if (!keyboardAvailable(context)) return S_OK;
    KeyEvent event = translateKey(wparam, lparam);
    keyDownVirtualKey_ = static_cast<UINT>(wparam);
    if (punctuationKeyboard_.isVisible()) {
        shiftTogglePending_ = false;
        if (!PunctuationModifier(event.virtualKey)) return runKeySession(context, std::move(event), eaten);
        return S_OK;
    }
    if (isShiftToggleKey(event)) {
        if (!shiftTogglePending_) shiftPressedAt_ = GetTickCount();
        shiftTogglePending_ = true;
        return S_OK;
    }
    shiftTogglePending_ = false;
    const HRESULT shortcutResult = handleFrontendShortcut(context, event, eaten);
    if (shortcutResult != S_FALSE) return shortcutResult;
    if (isModeToggleKey(event)) {
        toggleChineseMode();
        *eaten = TRUE;
        return S_OK;
    }
    if (isWidthToggleKey(event)) {
        toggleFullWidthMode();
        *eaten = TRUE;
        return S_OK;
    }
    if (!isPotentialKey(event)) return S_OK;
    return runKeySession(context, std::move(event), eaten);
}

HRESULT TextService::requestEditSession(ITfContext* context, ITfEditSession* session,
                                        HRESULT* editResult, bool* retriedAsync) {
    ReloadActivity activity(reloadActivity_);
    return RequestWriteEditSession([&](DWORD flags, HRESULT* result) {
        return context->RequestEditSession(clientId_, session, flags, result);
    }, editResult, retriedAsync);
}

HRESULT TextService::handleFrontendShortcut(ITfContext* context, const KeyEvent& event, BOOL* eaten) {
    ReloadActivity activity(reloadActivity_);
    const auto settings = CurrentFrontendSettings();
    switch (ShortcutFor(event, settings)) {
    case FrontendShortcut::NextInputMethod:
        selectInputMethod(NextInputMethod(CurrentInputMethod(), InputMethods(), settings));
        *eaten = TRUE;
        return S_OK;
    case FrontendShortcut::ToggleSimplified:
        toggleSimplifiedOutput();
        *eaten = TRUE;
        return S_OK;
    case FrontendShortcut::PunctuationKeyboard:
    case FrontendShortcut::RepeatCommit:
        return context ? runKeySession(context, event, eaten) : S_OK;
    default:
        return S_FALSE;
    }
}

HRESULT TextService::runKeySession(ITfContext* context, KeyEvent event, BOOL* eaten) {
    ReloadActivity activity(reloadActivity_);
    KeyEditActivity keyActivity(*this);
    auto* session = new (std::nothrow) KeyEditSession(this, context, std::move(event));
    if (!session) return E_OUTOFMEMORY;
    HRESULT editResult = E_FAIL;
    bool retriedAsync = false;
    HRESULT requestResult = requestEditSession(context, session, &editResult, &retriedAsync);
    if (retriedAsync) {
        // the session may run after this returns, so the key is claimed now
        if (SUCCEEDED(requestResult) && SUCCEEDED(editResult)) *eaten = TRUE;
    } else if (SUCCEEDED(requestResult) && SUCCEEDED(editResult)) {
        *eaten = session->handled();
    }
    if (FAILED(requestResult) || FAILED(editResult)) {
        Trace("KeyDown request=0x%08lX edit=0x%08lX", static_cast<unsigned long>(requestResult),
              static_cast<unsigned long>(editResult));
    }
    session->Release();
    // a transient lock error must not reach TSF; an unclaimed key goes to the app
    return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) {
    ReloadActivity activity(reloadActivity_);
    finishKeyPress(static_cast<UINT>(wparam));
    reloadKeyUpPending_ = false;
    reloadNotBefore_ = GetTickCount64() + 500;
    if (!eaten) return E_INVALIDARG;
    *eaten = FALSE;
    if (IsShiftKey(static_cast<UINT>(wparam)) && keyboardAvailable(context) &&
        engine_ && engine_->wantsShiftRelease()) {
        shiftTogglePending_ = false;
        shiftPressedAt_ = 0;
        KeyEvent event = translateKey(wparam, lparam);
        event.shift = false;
        event.keyUp = true;
        return runKeySession(context, event, eaten);
    }
    if (wparam == VK_CAPITAL && CurrentFrontendSettings().capsLockTogglesEnglish) {
        if (GetKeyState(VK_CAPITAL) & 1) requestCommitComposition();
        refreshLangBar();
        return S_OK;
    }
    if (IsShiftKey(static_cast<UINT>(wparam)) && shiftTogglePending_ &&
        !IsKeyDown(VK_CONTROL) && !IsKeyDown(VK_MENU) &&
        GetTickCount() - shiftPressedAt_ <= kShiftTapTimeoutMilliseconds) {
        shiftTogglePending_ = false;
        shiftPressedAt_ = 0;
        toggleChineseMode();
        *eaten = TRUE;
    } else if (IsShiftKey(static_cast<UINT>(wparam))) {
        shiftTogglePending_ = false;
        shiftPressedAt_ = 0;
    }
    return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) {
    ReloadActivity activity(reloadActivity_);
    reloadNotBefore_ = GetTickCount64() + 500;
    if (!eaten) return E_INVALIDARG;
    *eaten = FALSE;
    if (!keyboardAvailable(context)) return S_OK;
    if (!engine_ || !engine_->hasComposition()) RefreshSettings(true);
    if (guid == kSymbolWindowKeyGuid) {
        if (punctuationKeyboard_.isVisible() && context) {
            KeyEvent event;
            event.virtualKey = VK_OEM_PERIOD;
            event.control = event.alt = true;
            return runKeySession(context, event, eaten);
        }
        toggleSymbolWindow();
        *eaten = TRUE;
        return S_OK;
    }
    for (size_t index = 0; index < std::size(kPunctuationChordKeys); ++index) {
        if (guid != PunctuationChordGuid(index)) continue;
        KeyEvent event;
        event.virtualKey = kPunctuationChordKeys[index];
        event.control = true;
        event.alt = true;
        event.capsLock = (GetKeyState(VK_CAPITAL) & 1) != 0;
        event.numLock = (GetKeyState(VK_NUMLOCK) & 1) != 0;
        if (punctuationKeyboard_.isVisible() && context) return runKeySession(context, event, eaten);
        const HRESULT shortcutResult = handleFrontendShortcut(context, event, eaten);
        if (shortcutResult != S_FALSE) return shortcutResult;
        // an unclaimed chord goes on to the app, as in English mode
        if (!context || !isPotentialKey(event)) return S_OK;
        return runKeySession(context, std::move(event), eaten);
    }
    return S_OK;
}

HRESULT TextService::processKey(TfEditCookie editCookie, ITfContext* context,
                                const KeyEvent& event, bool* handled) {
    ReloadActivity activity(reloadActivity_);
    KeyEditActivity keyActivity(*this);
    reloadNotBefore_ = GetTickCount64() + 500;
    if (!context || !handled) return E_INVALIDARG;
    if (event.candidateIndex != static_cast<size_t>(-1) &&
        (!candidateActive_ || candidateContext_.Get() != context ||
         event.candidateGeneration != candidateGeneration_ || !keyboardAvailable(context))) {
        *handled = false;
        return S_OK;
    }
    const auto settings = CurrentFrontendSettings();
    if (punctuationKeyboard_.isVisible() && !PunctuationModifier(event.virtualKey)) {
        punctuationKeyboard_.close();
        *handled = true;
        const bool shiftedLetter = event.virtualKey >= 'A' && event.virtualKey <= 'Z';
        const wchar_t symbol = !event.control && !event.alt && (!event.shift || shiftedLetter)
                                   ? PunctuationSymbol(event.virtualKey) : 0;
        if (!symbol) {
            if (event.virtualKey != VK_ESCAPE) PlayTypingErrorSound(settings);
            return S_OK;
        }
        if (!engine_ || !isChineseMode()) return insertSymbol(editCookie, context, std::wstring(1, symbol));
        KeyEvent direct;
        direct.text.assign(1, symbol);
        direct.directText = true;
        const auto result = engine_->handleKey(direct);
        if (!result.handled) { PlayTypingErrorSound(settings); return S_OK; }
        return updateComposition(editCookie, context, result);
    }
    if (ShortcutFor(event, settings) == FrontendShortcut::PunctuationKeyboard) {
        *handled = true;
        if (candidateActive_) { PlayTypingErrorSound(settings); return S_OK; }
        ComPtr<ITfRange> range;
        TF_SELECTION selection{};
        ULONG fetched = 0;
        if (SUCCEEDED(context->GetSelection(editCookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) && fetched)
            range.Attach(selection.range);
        ComPtr<ITfContextView> view;
        RECT caret{};
        HWND owner = nullptr;
        BOOL clipped = FALSE;
        if (!range || FAILED(context->GetActiveView(&view)) ||
            FAILED(view->GetTextExt(editCookie, range.Get(), &caret, &clipped)) ||
            FAILED(view->GetWnd(&owner))) { PlayTypingErrorSound(settings); return S_OK; }
        presentAuxiliaryUI(punctuationUI_, 3,
            [this, owner, caret, follow = settings.keyboardFollowsCursor](bool show) {
                if (show) punctuationKeyboard_.open(owner, caret, follow);
                else punctuationKeyboard_.close();
            }, [this] { return punctuationKeyboard_.isVisible(); });
        return S_OK;
    }
    if (ShortcutFor(event, settings) == FrontendShortcut::RepeatCommit) {
        *handled = true;
        if (secureMode_) return S_OK;
        const bool composing = composition_ || (engine_ && engine_->hasComposition());
        if (composing) { PlayTypingErrorSound(settings); return S_OK; }
        // Repeat exactly what was sent, even if the conversion setting has changed.
        return commitText(editCookie, context, commitHistory_.replay(false), false);
    }
    if (composition_ && compositionContext_.Get() != context) {
        if (pendingModeCommit_) {
            *handled = false;
            return S_OK;
        }
        // a focus switch may never hand the old context a writable cookie
        abandonComposition();
    }
    EngineResult result;
    if (CapsLockAlphanumeric(event, settings)) {
        if (composition_) {
            const HRESULT commit = commitCompositionForModeSwitch(editCookie, context, true);
            if (FAILED(commit)) { *handled = false; return commit; }
        }
        const wchar_t character = AlphanumericCharacter(event, settings);
        if (!character) { *handled = false; return S_OK; }
        result.handled = true;
        result.committedText.assign(1, character);
        if (fullWidthMode_) result.committedText = ToFullWidth(std::move(result.committedText));
    } else if (!isChineseMode() && isFullWidthCharacterKey(event)) {
        result.handled = true;
        result.committedText = ToFullWidth(std::wstring(1, PrintableCharacter(event)));
    } else {
        if (!engine_) return E_UNEXPECTED;
        result = engine_->handleKey(event);
        if (fullWidthMode_ && !result.committedText.empty()) {
            result.committedText = ToFullWidth(std::move(result.committedText));
        }
    }
    *handled = result.handled;
    if (event.virtualKey == VK_SPACE || event.virtualKey == VK_TAB) {
        Trace("ProcessKey vk=%u core=%d handled=%d compositionUnits=%zu committedUnits=%zu cursor=%ld segment=%ld candidates=%d",
              event.virtualKey, MakeCoreKey(event).keyCode, result.handled, result.compositionText.size(),
              result.committedText.size(), result.compositionCursor, result.focusedSegment.length,
              result.candidatesVisible);
    }
    if (!result.handled) {
        // a filter can close its panel while passing the key on to the host
        updateCandidateWindow(editCookie, context, result);
        if (!isChineseMode() || !isFullWidthCharacterKey(event) ||
            !ApplyFullWidthFallback(PrintableCharacter(event), result)) return S_OK;
        *handled = true;
    }
    if (result.beep) PlayTypingErrorSound(settings);
    const HRESULT status = updateComposition(editCookie, context, result);
    if (FAILED(status)) {
        Trace("UpdateComposition hr=0x%08lX", static_cast<unsigned long>(status));
        *handled = false;
        abandonComposition();
    }
    return S_OK;
}

HRESULT TextService::ensureComposition(TfEditCookie editCookie, ITfContext* context) {
    ReloadActivity activity(reloadActivity_);
    if (composition_) return compositionContext_.Get() == context ? S_OK : E_UNEXPECTED;

    ComPtr<ITfInsertAtSelection> insertion;
    HRESULT result = context->QueryInterface(IID_PPV_ARGS(&insertion));
    if (FAILED(result)) { Trace("CompositionStart insertInterface=0x%08lX", result); return result; }
    ComPtr<ITfRange> range;
    result = insertion->InsertTextAtSelection(editCookie, TF_IAS_QUERYONLY, nullptr, 0, &range);
    if (FAILED(result)) { Trace("CompositionStart queryRange=0x%08lX", result); return result; }

    ComPtr<ITfContextComposition> compositionContext;
    result = context->QueryInterface(IID_PPV_ARGS(&compositionContext));
    if (FAILED(result)) return result;
    result = compositionContext->StartComposition(editCookie, range.Get(), this, &composition_);
    if (FAILED(result)) Trace("CompositionStart start=0x%08lX", result);
    if (SUCCEEDED(result) && composition_) {
        compositionContext_ = context;
    } else if (SUCCEEDED(result)) {
        // a read-only field refuses the composition with S_OK and no object
        result = E_FAIL;
        Trace("CompositionStart hostRefused=1");
    }
    return result;
}

void TextService::applyDisplayAttributes(TfEditCookie editCookie, ITfContext* context,
                                         ITfRange* range, const EngineResult& result) {
    ReloadActivity activity(reloadActivity_);
    if (inputAttributeAtom_ == TF_INVALID_GUIDATOM) return;
    ComPtr<ITfProperty> property;
    if (FAILED(context->GetProperty(GUID_PROP_ATTRIBUTE, &property))) return;

    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4;
    value.lVal = static_cast<LONG>(inputAttributeAtom_);
    property->SetValue(editCookie, range, &value);

    const CompositionSegment& focused = result.focusedSegment;
    if (focusedAttributeAtom_ == TF_INVALID_GUIDATOM || focused.length <= 0) return;
    ComPtr<ITfRange> segment;
    if (FAILED(range->Clone(&segment))) return;
    LONG shifted = 0;
    segment->Collapse(editCookie, TF_ANCHOR_START);
    segment->ShiftEnd(editCookie, focused.start + focused.length, &shifted, nullptr);
    segment->ShiftStart(editCookie, focused.start, &shifted, nullptr);
    value.lVal = static_cast<LONG>(focusedAttributeAtom_);
    property->SetValue(editCookie, segment.Get(), &value);
}

HRESULT TextService::replaceCompositionText(TfEditCookie editCookie, ITfContext* context,
                                            const EngineResult& result) {
    ReloadActivity activity(reloadActivity_);
    HRESULT status = ensureComposition(editCookie, context);
    if (FAILED(status)) return status;
    ComPtr<ITfRange> range;
    status = composition_->GetRange(&range);
    if (FAILED(status)) return status;
    const std::wstring& text = result.compositionText;
    status = range->SetText(editCookie, 0, text.data(), static_cast<LONG>(text.size()));
    if (FAILED(status)) return status;

    applyDisplayAttributes(editCookie, context, range.Get(), result);

    ComPtr<ITfRange> selection;
    status = range->Clone(&selection);
    if (FAILED(status)) return status;
    selection->Collapse(editCookie, TF_ANCHOR_START);
    LONG shifted = 0;
    selection->ShiftEnd(editCookie,
                        std::clamp<LONG>(result.compositionCursor, 0,
                                         static_cast<LONG>(text.size())),
                        &shifted, nullptr);
    selection->Collapse(editCookie, TF_ANCHOR_END);
    return MoveCaret(editCookie, context, selection.Get());
}

void TextService::recordCommittedText(const std::wstring& text) {
    ReloadActivity activity(reloadActivity_);
    if (text.empty() || secureMode_) return;
    if (!historyHostReady_ && !historyHostPath_.empty() && GetTickCount64() >= nextHistoryHostAttempt_) {
        nextHistoryHostAttempt_ = GetTickCount64() + 10000;
        historyHostReady_ = EnsureHistoryHost(historyHostPath_);
        if (!historyHostReady_) Trace("History: session host unavailable; retaining TIP-local mapping");
    }
    commitHistory_.record(text);
    if (CurrentFrontendSettings().wordCountEnabled &&
        !AddWordCount(CurrentWritablePath(), text, LocalDayNumber(), 200))
        Trace("WordCount: write unavailable");
}

HRESULT TextService::commitText(TfEditCookie editCookie, ITfContext* context,
                                const std::wstring& text, bool filter) {
    ReloadActivity activity(reloadActivity_);
    if (text.empty()) return S_OK;
    pendingCommitText_.clear();
    const std::wstring output = filter
        ? FilterCommittedText(text, CurrentFrontendSettings().simplifiedOutput) : text;
    if (composition_ && compositionContext_.Get() == context) {
        ComPtr<ITfRange> range;
        HRESULT result = composition_->GetRange(&range);
        if (FAILED(result)) return result;
        result = range->SetText(editCookie, 0, output.data(), static_cast<LONG>(output.size()));
        if (FAILED(result)) return result;
        pendingCommitText_ = output;
        // not every host moves the caret on SetText; the next word would land in front
        result = range->Collapse(editCookie, TF_ANCHOR_END);
        if (FAILED(result)) return result;
        result = MoveCaret(editCookie, context, range.Get());
        if (FAILED(result)) return result;
        return endComposition(editCookie, false);
    }

    ComPtr<ITfInsertAtSelection> insertion;
    HRESULT result = context->QueryInterface(IID_PPV_ARGS(&insertion));
    if (FAILED(result)) return result;
    ComPtr<ITfRange> insertedRange;
    result = insertion->InsertTextAtSelection(editCookie, 0, output.data(),
                                              static_cast<LONG>(output.size()), &insertedRange);
    if (FAILED(result)) return result;
    recordCommittedText(output);
    if (!insertedRange) return E_UNEXPECTED;
    result = insertedRange->Collapse(editCookie, TF_ANCHOR_END);
    if (FAILED(result)) return result;
    return MoveCaret(editCookie, context, insertedRange.Get());
}

HRESULT TextService::convertCompositionForCommit(TfEditCookie editCookie) {
    ReloadActivity activity(reloadActivity_);
    if (!composition_) return S_OK;
    ComPtr<ITfRange> range, reader;
    HRESULT result = composition_->GetRange(&range);
    if (FAILED(result)) return result;
    result = range->Clone(&reader);
    if (FAILED(result)) return result;
    std::wstring text;
    std::array<wchar_t, 256> buffer{};
    ULONG fetched = 0;
    do {
        result = reader->GetText(editCookie, TF_TF_MOVESTART, buffer.data(),
                                 static_cast<ULONG>(buffer.size()), &fetched);
        if (FAILED(result)) return result;
        text.append(buffer.data(), fetched);
    } while (fetched != 0);
    const std::wstring output = FilterCommittedText(text, CurrentFrontendSettings().simplifiedOutput);
    if (output != text) {
        result = range->SetText(editCookie, 0, output.data(), static_cast<LONG>(output.size()));
        if (FAILED(result)) return result;
    }
    pendingCommitText_ = output;
    return S_OK;
}

HRESULT TextService::endComposition(TfEditCookie editCookie, bool clearText) {
    ReloadActivity activity(reloadActivity_);
    if (!composition_) return S_OK;
    ComPtr<ITfRange> range;
    if (compositionContext_ && SUCCEEDED(composition_->GetRange(&range)) && range) {
        ComPtr<ITfProperty> property;
        if (SUCCEEDED(compositionContext_->GetProperty(GUID_PROP_ATTRIBUTE, &property))) {
            property->Clear(editCookie, range.Get());
        }
        if (clearText) range->SetText(editCookie, 0, nullptr, 0);
    }
    endingComposition_ = true;
    ComPtr<ITfComposition> completing = composition_;
    const HRESULT result = completing->EndComposition(editCookie);
    endingComposition_ = false;
    if (SUCCEEDED(result)) {
        if (!clearText && !pendingCommitText_.empty()) recordCommittedText(pendingCommitText_);
        pendingCommitText_.clear();
        composition_.Reset();
        compositionContext_.Reset();
    }
    return result;
}

void TextService::resetCandidateState() {
    ReloadActivity activity(reloadActivity_);
    ++candidateGeneration_;
    candidateUI_.end();
    candidateWindow_.hide();
    candidateActive_ = false;
    candidateAnchor_.Reset();
    candidateContext_.Reset();
}

bool TextService::requestCommitComposition(bool moveCaret) {
    ReloadActivity activity(reloadActivity_);
    if (pendingModeCommit_) return true;
    if (!composition_) {
        resetCandidateState();
        if (engine_) engine_->reset();
        return true;
    }
    if (!compositionContext_ || clientId_ == TF_CLIENTID_NULL) return false;

    auto* session = new (std::nothrow) CommitModeSwitchEditSession(
        this, compositionContext_.Get(), moveCaret, ++commitGeneration_);
    if (!session) return false;
    pendingModeCommit_ = true;
    HRESULT editResult = E_FAIL;
    HRESULT requestResult = requestEditSession(compositionContext_.Get(), session, &editResult);
    const bool accepted = SUCCEEDED(requestResult) && SUCCEEDED(editResult);
    if (!accepted) {
        pendingModeCommit_ = false;
        Trace("ModeCommit request=0x%08lX edit=0x%08lX",
              static_cast<unsigned long>(requestResult),
              static_cast<unsigned long>(editResult));
    }
    session->Release();
    return accepted;
}

HRESULT TextService::commitCompositionForModeSwitch(TfEditCookie editCookie,
                                                    ITfContext* context, bool moveCaret) {
    ReloadActivity activity(reloadActivity_);
    HRESULT result = S_OK;
    if (composition_ && compositionContext_.Get() == context) {
        result = convertCompositionForCommit(editCookie);
        // EndComposition keeps the text; abandoning would clear the user's sentence
        if (SUCCEEDED(result) && moveCaret) {
            ComPtr<ITfRange> range;
            result = composition_->GetRange(&range);
            ComPtr<ITfRange> caret;
            if (SUCCEEDED(result)) result = range->Clone(&caret);
            if (SUCCEEDED(result)) result = caret->Collapse(editCookie, TF_ANCHOR_END);
            if (SUCCEEDED(result)) result = MoveCaret(editCookie, context, caret.Get());
        }
        if (SUCCEEDED(result)) result = endComposition(editCookie, false);
    }
    if (SUCCEEDED(result)) {
        resetCandidateState();
        if (engine_) engine_->reset();
    }
    pendingModeCommit_ = false;
    if (FAILED(result) && !chineseMode_ && threadManager_) setChineseMode(true);
    return result;
}

void TextService::commitSessionDropped(ITfContext* context, unsigned generation) {
    ReloadActivity activity(reloadActivity_);
    if (!pendingModeCommit_ || generation != commitGeneration_) return;
    pendingModeCommit_ = false;
    // the context took its composition with it
    if (compositionContext_.Get() == context) {
        composition_.Reset();
        compositionContext_.Reset();
    }
    resetCandidateState();
    if (engine_) engine_->reset();
}

void TextService::abandonComposition() {
    ReloadActivity activity(reloadActivity_);
    resetCandidateState();
    if (engine_) engine_->reset();

    ComPtr<ITfComposition> oldComposition = composition_;
    ComPtr<ITfContext> oldContext = compositionContext_;
    composition_.Reset();
    compositionContext_.Reset();
    pendingCommitText_.clear();

    if (!oldComposition || !oldContext || clientId_ == TF_CLIENTID_NULL) return;
    auto* session = new (std::nothrow) TerminateEditSession(this, oldComposition.Get());
    if (!session) return;
    HRESULT editResult = E_FAIL;
    oldContext->RequestEditSession(clientId_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE,
                                   &editResult);
    session->Release();
}

HRESULT TextService::updateComposition(TfEditCookie editCookie, ITfContext* context,
                                       const EngineResult& result) {
    ReloadActivity activity(reloadActivity_);
    HRESULT status = commitText(editCookie, context, result.committedText);
    if (FAILED(status)) return status;

    if (!result.compositionText.empty()) {
        status = replaceCompositionText(editCookie, context, result);
        if (FAILED(status)) return status;
    } else if (composition_ && result.committedText.empty()) {
        status = endComposition(editCookie, true);
        if (FAILED(status)) return status;
    }
    updateCandidateWindow(editCookie, context, result);
    if (!secureMode_ && !result.notification.empty()) showNotification(result.notification);
    return S_OK;
}

void TextService::presentAuxiliaryUI(UIElementSession& session, int kind,
                                     std::function<void(bool)> show, std::function<bool()> visible) {
    ReloadActivity activity(reloadActivity_);
    // Optional palettes and notifications must not pop over an exclusive game.
    if (secureMode_ || uiLessMode_) { session.end(); show(false); return; }
    ComPtr<TextUIElement> element = session.element();
    if (!element) {
        GUID guid = {0xd0126bd3, 0x5ecf, 0x4dcb, {0x91, 0x16, 0xad, 0xbb, 0x61, 0xa2, 0x9c, 0}};
        guid.Data4[7] = static_cast<BYTE>(kind);
        try {
            element.Attach(new TextUIElement(false, guid, L"ChiaKey auxiliary UI"));
        } catch (const std::bad_alloc&) {
            show(false);
            return;
        }
    }
    element->setCallbacks(std::move(show), std::move(visible));
    const HRESULT status = session.present(uiElementManager_.Get(), element.Get(), true);
    if (FAILED(status)) Trace("AuxiliaryUI kind=%d hr=0x%08lX", kind, status);
}

void TextService::showNotification(const std::wstring& text) {
    ReloadActivity activity(reloadActivity_);
    presentAuxiliaryUI(notificationUI_, 1,
        [this, text](bool show) { if (show) notificationWindow_.show(text); else notificationWindow_.hide(); },
        [this] { return notificationWindow_.isVisible(); });
}

void TextService::showSymbolWindow(bool userOpened) {
    ReloadActivity activity(reloadActivity_);
    presentAuxiliaryUI(symbolUI_, 2,
        [this, userOpened](bool show) {
            symbolWindow_.setHostAllowed(show);
            if (!show) symbolWindow_.hide();
            else if (userOpened) symbolWindow_.open();
            else symbolWindow_.show();
        }, [this] { return symbolWindow_.isVisible(); });
}

bool TextService::keyboardAvailable(ITfContext* context) const {
    ReloadActivity activity(reloadActivity_);
    if (!context) return false;
    TF_STATUS status{};
    if (SUCCEEDED(context->GetStatus(&status)) && (status.dwDynamicFlags & TF_SD_READONLY)) return false;
    ComPtr<ITfCompartmentMgr> compartments;
    if (FAILED(context->QueryInterface(IID_PPV_ARGS(&compartments)))) return true;
    for (const auto& guid : {GUID_COMPARTMENT_KEYBOARD_DISABLED, GUID_COMPARTMENT_EMPTYCONTEXT}) {
        ComPtr<ITfCompartment> compartment;
        VARIANT value; VariantInit(&value);
        bool disabled = false;
        if (SUCCEEDED(compartments->GetCompartment(guid, &compartment)) &&
            SUCCEEDED(compartment->GetValue(&value)))
            disabled = value.vt == VT_I4 && value.lVal != 0;
        VariantClear(&value);
        if (disabled) return false;
    }
    return true;
}

void TextService::updateCandidateWindow(TfEditCookie editCookie, ITfContext* context,
                                        const EngineResult& result) {
    ReloadActivity activity(reloadActivity_);
    const bool showCandidates = result.candidatesVisible && !result.candidates.empty();
    if (!showCandidates && result.message.empty()) { resetCandidateState(); return; }

    // Candidate data remains available even if a game's text store has no geometry.
    ComPtr<ITfDocumentMgr> document;
    context->GetDocumentMgr(&document);
    ComPtr<ITfRange> range;
    if (composition_) composition_->GetRange(&range);
    if (!range) {
        TF_SELECTION selection{}; ULONG fetched = 0;
        if (SUCCEEDED(context->GetSelection(editCookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) && fetched)
            range.Attach(selection.range);
    }
    ComPtr<ITfContextView> view;
    RECT textRect{}; BOOL clipped = FALSE; HWND owner = nullptr;
    bool anchored = false;
    if (SUCCEEDED(context->GetActiveView(&view)) && view) {
        view->GetWnd(&owner);
        const HRESULT geometry = range ? view->GetTextExt(editCookie, range.Get(), &textRect, &clipped) : E_FAIL;
        anchored = SUCCEEDED(geometry) && owner && textRect.bottom > textRect.top;
        if (!anchored) {
            Trace("CandidateGeometry hr=0x%08lX owner=%d clipped=%d uiLess=%d", geometry, owner != nullptr, clipped, uiLessMode_);
            // Legacy custom controls sometimes expose a system caret but no TSF layout.
            GUITHREADINFO info{sizeof(info)};
            if (owner && GetGUIThreadInfo(GetWindowThreadProcessId(owner, nullptr), &info) && info.hwndCaret) {
                textRect = info.rcCaret;
                MapWindowPoints(info.hwndCaret, HWND_DESKTOP, reinterpret_cast<POINT*>(&textRect), 2);
                anchored = textRect.bottom > textRect.top;
            }
            if (!anchored && owner && GetClientRect(owner, &textRect)) {
                textRect.left += 8; textRect.right = textRect.left + 1;
                textRect.top = std::max(textRect.top, textRect.bottom - 32);
                MapWindowPoints(owner, HWND_DESKTOP, reinterpret_cast<POINT*>(&textRect), 2);
                anchored = textRect.bottom > textRect.top;
            }
        }
    }

    ComPtr<TextUIElement> element = candidateUI_.element();
    if (element) {
        ComPtr<ITfCandidateListUIElement> list;
        const bool wasCandidates = SUCCEEDED(element.As(&list));
        if (wasCandidates != showCandidates) { candidateUI_.end(); element.Reset(); }
    }
    if (!element) {
        GUID guid = {0x9d26f572, 0x1a99, 0x4948, {0x90, 0xc9, 0x94, 0xe2, 0xf2, 0x8c, 0x57, 0x22}};
        if (!showCandidates) ++guid.Data4[7];
        try {
            element.Attach(new TextUIElement(showCandidates, guid, showCandidates ? L"ChiaKey candidates" : L"ChiaKey message"));
        } catch (const std::bad_alloc&) {
            resetCandidateState();
            return;
        }
    }
    element->update(document.Get(), result.allCandidates, static_cast<UINT>(result.selectedCandidate),
                    static_cast<UINT>(result.candidatesPerPage));
    candidateContext_ = context;
    candidateActive_ = showCandidates;
    const unsigned generation = ++candidateGeneration_;
    element->setCallbacks([this, owner, textRect, result, anchored, showCandidates, generation](bool show) {
        if (!show || !anchored || secureMode_) { candidateWindow_.hide(); return; }
        if (showCandidates) {
            candidateWindow_.show(owner, textRect, result);
            // A no-activate click keeps the text field focused. Capture its
            // context, and let TSF grant a write lock before accepting it.
            ComPtr<ITfContext> target = candidateContext_;
            candidateWindow_.setSelectionCallback([this, target, generation, first =
                (result.candidatePage - 1) * result.candidatesPerPage](size_t index) {
                if (!target || !keyboardAvailable(target.Get()) || candidateContext_ != target) return;
                KeyEvent event;
                event.candidateIndex = first + index;
                event.candidateGeneration = generation;
                BOOL eaten = FALSE;
                runKeySession(target.Get(), event, &eaten);
            });
        }
        else candidateWindow_.showMessage(owner, textRect, result.message);
    }, [this] { return candidateWindow_.isVisible(); });
    const HRESULT uiStatus = candidateUI_.present(uiElementManager_.Get(), element.Get(), !uiLessMode_ && !secureMode_);
    if (FAILED(uiStatus)) Trace("CandidateUI hr=0x%08lX uiLess=%d", uiStatus, uiLessMode_);
    candidateAnchor_.Reset();
    if (showCandidates && !composition_ && range && SUCCEEDED(range->Clone(&candidateAnchor_)))
        candidateAnchor_->Collapse(editCookie, TF_ANCHOR_END);
}
bool TextService::selectionMatchesTrackedState(TfEditCookie editCookie,
                                               ITfContext* context) const {
    ReloadActivity activity(reloadActivity_);
    if (!context) return false;

    TF_SELECTION selection{};
    ULONG fetched = 0;
    if (FAILED(context->GetSelection(editCookie, TF_DEFAULT_SELECTION, 1, &selection,
                                     &fetched)) ||
        !fetched) {
        return false;
    }
    ComPtr<ITfRange> selected;
    selected.Attach(selection.range);
    BOOL empty = FALSE;
    if (FAILED(selected->IsEmpty(editCookie, &empty)) || !empty) return false;

    ComPtr<ITfRange> tracked;
    bool allowsPositionInsideRange = false;
    if (composition_ && compositionContext_.Get() == context) {
        if (FAILED(composition_->GetRange(&tracked)) || !tracked) return false;
        allowsPositionInsideRange = true;
    } else if (candidateActive_ && candidateAnchor_) {
        tracked = candidateAnchor_;
    } else {
        return true;
    }

    LONG comparedWithStart = 0;
    LONG comparedWithEnd = 0;
    if (FAILED(selected->CompareStart(editCookie, tracked.Get(), TF_ANCHOR_START,
                                      &comparedWithStart)) ||
        FAILED(selected->CompareStart(editCookie, tracked.Get(), TF_ANCHOR_END,
                                      &comparedWithEnd))) {
        return false;
    }
    if (allowsPositionInsideRange) return comparedWithStart >= 0 && comparedWithEnd <= 0;
    return comparedWithStart == 0;
}

STDMETHODIMP TextService::OnEndEdit(ITfContext* context, TfEditCookie editCookie,
                                    ITfEditRecord* editRecord) {
    ReloadActivity activity(reloadActivity_);
    if (!context || !editRecord || (!composition_ && !candidateActive_)) return S_OK;

    BOOL selectionChanged = FALSE;
    if (FAILED(editRecord->GetSelectionStatus(&selectionChanged)) || !selectionChanged) {
        return S_OK;
    }
    if (!selectionMatchesTrackedState(editCookie, context) && !pendingModeCommit_) {
        // keep the sentence, and leave the caret where the user put it
        if (!requestCommitComposition(false)) abandonComposition();
    }
    return S_OK;
}

STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie editCookie, ITfComposition* composition) {
    ReloadActivity activity(reloadActivity_);
    if (composition_.Get() == composition) {
        if (!endingComposition_) {
            const HRESULT conversion = convertCompositionForCommit(editCookie);
            if (SUCCEEDED(conversion)) { recordCommittedText(pendingCommitText_); pendingCommitText_.clear(); }
            if (FAILED(conversion)) Trace("Host termination conversion failed: 0x%08lX", conversion);
        }
        composition_.Reset();
        compositionContext_.Reset();
        if (!endingComposition_) {
            resetCandidateState();
            if (engine_) engine_->reset();
        }
    }
    return S_OK;
}

STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr*) {
    ReloadActivity activity(reloadActivity_);
    return S_OK;
}

STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr* documentManager) {
    ReloadActivity activity(reloadActivity_);
    if (compositionContext_ && !pendingModeCommit_) {
        ComPtr<ITfDocumentMgr> owner;
        if (SUCCEEDED(compositionContext_->GetDocumentMgr(&owner)) &&
            owner.Get() == documentManager) {
            requestCommitComposition();
        }
    }
    if (textEditContext_) {
        ComPtr<ITfDocumentMgr> owner;
        if (SUCCEEDED(textEditContext_->GetDocumentMgr(&owner)) &&
            owner.Get() == documentManager) {
            if (!pendingModeCommit_) requestCommitComposition();
            unadviseTextEditSink();
        }
    }
    return S_OK;
}

STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr* focused, ITfDocumentMgr*) {
    ReloadActivity activity(reloadActivity_);
    if (keyEditDepth_ || (keyDownVirtualKey_ &&
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr) == GetCurrentThreadId())) {
        deferredDocumentFocus_ = true;
        Trace("Document focus deferred during key input");
        return S_OK;
    }
    keyDownVirtualKey_ = 0;
    deferredDocumentFocus_ = false;
    ComPtr<ITfContext> focusedContext;
    if (focused) focused->GetTop(&focusedContext);
    if (focusedContext.Get() != textEditContext_.Get()) punctuationKeyboard_.close();
    ITfContext* trackedContext = composition_ ? compositionContext_.Get() : candidateContext_.Get();
    if ((composition_ || candidateActive_) &&
        trackedContext != focusedContext.Get() && !pendingModeCommit_) {
        if (!requestCommitComposition()) {
            Trace("Document focus changed: composition could not be committed");
        }
    }
    adviseTextEditSink(focusedContext.Get());
    // coming back from the settings app is when a change is expected to show
    if (focused && !composition_ && !candidateActive_) RefreshSettings();
    if (!focused) {
        resetCandidateState();
        if (engine_) engine_->reset();
    }
    return S_OK;
}

STDMETHODIMP TextService::OnPushContext(ITfContext* context) {
    ReloadActivity activity(reloadActivity_);
    if ((composition_ || candidateActive_) && textEditContext_.Get() != context &&
        !pendingModeCommit_) {
        requestCommitComposition();
    }
    // a host without ITfSource still needs the push to succeed
    adviseTextEditSink(context);
    return S_OK;
}

STDMETHODIMP TextService::OnPopContext(ITfContext* context) {
    ReloadActivity activity(reloadActivity_);
    if (compositionContext_.Get() == context) {
        if (!pendingModeCommit_) requestCommitComposition();
    } else if (!composition_ && candidateContext_.Get() == context) {
        // a composition in another context is not this pop's to clear
        resetCandidateState();
        if (engine_) engine_->reset();
    }
    if (textEditContext_.Get() == context) {
        unadviseTextEditSink();
        ComPtr<ITfDocumentMgr> focused;
        ComPtr<ITfContext> top;
        if (threadManager_ && SUCCEEDED(threadManager_->GetFocus(&focused)) && focused &&
            SUCCEEDED(focused->GetTop(&top)) && top.Get() != context) {
            adviseTextEditSink(top.Get());
        }
    }
    return S_OK;
}

// one window per app thread, brought up where the user goes as Yahoo's single window was;
// the window itself hides when another app comes to the front
STDMETHODIMP TextService::OnSetThreadFocus() {
    ReloadActivity activity(reloadActivity_);
    RefreshSettings();
    if (!symbolWindow_.isVisible() && ReadSymbolWindowState().visible) showSymbolWindow();
    return S_OK;
}

STDMETHODIMP TextService::OnKillThreadFocus() {
    ReloadActivity activity(reloadActivity_);
    // Showing nonactivating IME UI can itself trigger this callback in some hosts.
    if (GetWindowThreadProcessId(GetForegroundWindow(), nullptr) != GetCurrentThreadId())
        punctuationKeyboard_.close();
    return S_OK;
}

STDMETHODIMP TextService::OnChange(REFGUID guid) {
    ReloadActivity activity(reloadActivity_);
    if (!threadManager_) return S_OK;
    if (guid != GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION &&
        guid != GUID_COMPARTMENT_KEYBOARD_OPENCLOSE) {
        return S_OK;
    }

    ComPtr<ITfCompartmentMgr> manager;
    ComPtr<ITfCompartment> compartment;
    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(threadManager_.As(&manager)) &&
        SUCCEEDED(manager->GetCompartment(guid, &compartment)) &&
        SUCCEEDED(compartment->GetValue(&value)) && value.vt == VT_I4) {
        if (guid == GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION) {
            const bool width = (value.lVal & static_cast<LONG>(TF_CONVERSIONMODE_FULLSHAPE)) != 0;
            if (fullWidthMode_ != width) setFullWidthMode(width);
        } else if (chineseMode_ != (value.lVal != 0)) {
            setChineseMode(value.lVal != 0);
        }
    }
    VariantClear(&value);
    return S_OK;
}

STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** items) {
    ReloadActivity activity(reloadActivity_);
    if (!items) return E_INVALIDARG;
    *items = new (std::nothrow) DisplayAttributeEnum();
    return *items ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guid,
                                                  ITfDisplayAttributeInfo** info) {
    ReloadActivity activity(reloadActivity_);
    if (!info) return E_INVALIDARG;
    *info = nullptr;
    for (const DisplayAttributeSpec& spec : kDisplayAttributes) {
        if (guid == *spec.guid) {
            *info = new (std::nothrow) DisplayAttributeInfo(spec);
            return *info ? S_OK : E_OUTOFMEMORY;
        }
    }
    return E_INVALIDARG;
}

}  // namespace ChiaKey::WindowsTsf
