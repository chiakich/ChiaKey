#include "TextService.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <new>
#include <shellapi.h>
#include <utility>

#include "Diagnostics.h"
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
        service_->AddRef();
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
    ~KeyEditSession() { service_->Release(); }
    std::atomic<ULONG> references_{1};
    TextService* service_;
    ComPtr<ITfContext> context_;
    KeyEvent event_;
    bool handled_ = false;
};

class TerminateEditSession final : public ITfEditSession {
public:
    explicit TerminateEditSession(ITfComposition* composition) : composition_(composition) {}
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
    ~TerminateEditSession() = default;
    std::atomic<ULONG> references_{1};
    ComPtr<ITfComposition> composition_;
};

class CommitModeSwitchEditSession final : public ITfEditSession {
public:
    CommitModeSwitchEditSession(TextService* service, ITfContext* context, bool moveCaret,
                                unsigned generation)
        : service_(service), context_(context), moveCaret_(moveCaret), generation_(generation) {
        service_->AddRef();
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
        service_->Release();
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
        service_->AddRef();
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
    ~SymbolEditSession() { service_->Release(); }
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

TextService::TextService() { ++g_objectCount; }
TextService::~TextService() { --g_objectCount; }

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

STDMETHODIMP TextService::Activate(ITfThreadMgr* threadManager, TfClientId clientId) {
    return ActivateEx(threadManager, clientId, 0);
}

STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* threadManager, TfClientId clientId,
                                     DWORD flags) {
    if (!threadManager || clientId == TF_CLIENTID_NULL) return E_INVALIDARG;
    if (threadManager_) return S_OK;

    secureMode_ = (flags & TF_TMAE_SECUREMODE) != 0;
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
            symbolWindow_.show();
        }
    } else {
        Trace("AdviseSinks hr=0x%08lX", static_cast<unsigned long>(result));
        unadviseSinks();
        uninitializeLangBar();
        engine_.reset();
        threadManager_.Reset();
        clientId_ = TF_CLIENTID_NULL;
    }
    return result;
}

STDMETHODIMP TextService::Deactivate() {
    if (!requestCommitComposition()) {
        Trace("Deactivate: composition could not be committed");
    }
    unadviseFunctionProvider();
    unadviseSinks();
    uninitializeLangBar();
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
    if (textEditContext_ && textEditCookie_ != TF_INVALID_COOKIE) {
        ComPtr<ITfSource> source;
        if (SUCCEEDED(textEditContext_.As(&source))) source->UnadviseSink(textEditCookie_);
    }
    textEditCookie_ = TF_INVALID_COOKIE;
    textEditContext_.Reset();
}

HRESULT TextService::adviseInputModeSink() {
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
    ComPtr<ITfSourceSingle> source;
    HRESULT result = threadManager_.As(&source);
    if (FAILED(result)) return result;
    return source->AdviseSingleSink(clientId_, IID_ITfFunctionProvider,
                                    static_cast<ITfFunctionProvider*>(this));
}

void TextService::unadviseFunctionProvider() {
    if (!threadManager_ || clientId_ == TF_CLIENTID_NULL) return;
    ComPtr<ITfSourceSingle> source;
    if (SUCCEEDED(threadManager_.As(&source))) {
        source->UnadviseSingleSink(clientId_, IID_ITfFunctionProvider);
    }
}

HRESULT TextService::openSettings(HWND parent, const wchar_t* arguments) const {
    const HINSTANCE launched = ShellExecuteW(parent, L"open", SettingsAppPath().c_str(), arguments,
                                             nullptr, SW_SHOWNORMAL);
    const INT_PTR code = reinterpret_cast<INT_PTR>(launched);
    return code > 32 ? S_OK : HRESULT_FROM_WIN32(static_cast<DWORD>(code));
}

STDMETHODIMP TextService::GetType(GUID* guid) {
    if (!guid) return E_INVALIDARG;
    *guid = kTextServiceClsid;
    return S_OK;
}

STDMETHODIMP TextService::GetDescription(BSTR* description) {
    if (!description) return E_INVALIDARG;
    *description = SysAllocString(kTextServiceDescription);
    return *description ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::GetFunction(REFGUID guid, REFIID iid, IUnknown** object) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (guid != GUID_NULL || iid != IID_ITfFnConfigure) return E_NOINTERFACE;
    return QueryInterface(iid, reinterpret_cast<void**>(object));
}

STDMETHODIMP TextService::GetDisplayName(BSTR* name) {
    if (!name) return E_INVALIDARG;
    *name = SysAllocString(L"千秋輸入法設定");
    return *name ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::Show(HWND parent, LANGID, REFGUID) { return openSettings(parent); }

HRESULT TextService::initializeLangBar() {
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
        result = manager->AddItem(button);
        if (FAILED(result)) {
            uninitializeLangBar();
            return result;
        }
    }
    return S_OK;
}

void TextService::uninitializeLangBar() {
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
        if (manager) manager->RemoveItem(button);
        button->Release();
    }
}

void TextService::refreshLangBar() {
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
    if (secureMode_ || !CurrentFrontendSettings().showNotifications) return;
    BOOL focused = FALSE;
    if (threadManager_ && SUCCEEDED(threadManager_->IsThreadFocus(&focused)) && focused)
        notificationWindow_.show(text);
}

void TextService::setChineseMode(bool enabled) {
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

void TextService::toggleChineseMode() { setChineseMode(!chineseMode_); }

void TextService::setFullWidthMode(bool enabled) {
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

void TextService::toggleFullWidthMode() { setFullWidthMode(!fullWidthMode_); }

bool TextService::toggleSimplifiedOutput() {
    RefreshSettings();
    const bool changed = SetSimplifiedOutput(!CurrentFrontendSettings().simplifiedOutput);
    if (changed) {
        refreshLangBar();
        notifyMode(UiText(CurrentFrontendSettings().simplifiedOutput ? L"簡體中文輸出" : L"繁體中文輸出"));
    }
    return changed;
}

bool TextService::selectInputMethod(const std::string& identifier) {
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
    if (!event.control || event.alt) return false;
    return event.virtualKey == VK_SPACE;
}

bool TextService::isShiftToggleKey(const KeyEvent& event) const {
    return IsShiftKey(event.virtualKey) && !event.control && !event.alt &&
           CurrentFrontendSettings().shiftTogglesEnglish &&
           !CurrentFrontendSettings().capsLockTogglesEnglish;
}

bool TextService::isWidthToggleKey(const KeyEvent& event) const {
    return event.virtualKey == VK_SPACE && event.shift && !event.control && !event.alt;
}

bool TextService::isFullWidthCharacterKey(const KeyEvent& event) const {
    return fullWidthMode_ && !event.control && !event.alt && PrintableCharacter(event) != 0;
}

void TextService::toggleSymbolWindow() {
    if (symbolWindow_.isVisible()) {
        symbolWindow_.close();
    } else {
        symbolWindow_.open();
    }
    refreshLangBar();
}

void TextService::sendSymbol(const std::wstring& text) {
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

STDMETHODIMP TextService::OnTestKeyDown(ITfContext*, WPARAM wparam, LPARAM lparam,
                                        BOOL* eaten) {
    if (!eaten) return E_INVALIDARG;
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

STDMETHODIMP TextService::OnTestKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    if (!eaten) return E_INVALIDARG;
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
    if (!context || !eaten) return E_INVALIDARG;
    *eaten = FALSE;
    KeyEvent event = translateKey(wparam, lparam);
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
    *editResult = E_FAIL;
    HRESULT requestResult = context->RequestEditSession(
        clientId_, session, TF_ES_SYNC | TF_ES_READWRITE, editResult);
    const bool refused = requestResult == TF_E_SYNCHRONOUS || requestResult == TF_E_LOCKED ||
                         (SUCCEEDED(requestResult) && *editResult == TF_E_SYNCHRONOUS);
    if (refused) {
        *editResult = E_FAIL;
        requestResult = context->RequestEditSession(
            clientId_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, editResult);
    }
    if (retriedAsync) *retriedAsync = refused;
    return requestResult;
}

HRESULT TextService::handleFrontendShortcut(ITfContext* context, const KeyEvent& event, BOOL* eaten) {
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

STDMETHODIMP TextService::OnKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
    if (!eaten) return E_INVALIDARG;
    *eaten = FALSE;
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
    if (!eaten) return E_INVALIDARG;
    *eaten = FALSE;
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
    if (!context || !handled) return E_INVALIDARG;
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
        punctuationKeyboard_.open(owner, caret, settings.keyboardFollowsCursor);
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
    if (composition_) return compositionContext_.Get() == context ? S_OK : E_UNEXPECTED;

    ComPtr<ITfInsertAtSelection> insertion;
    HRESULT result = context->QueryInterface(IID_PPV_ARGS(&insertion));
    if (FAILED(result)) return result;
    ComPtr<ITfRange> range;
    result = insertion->InsertTextAtSelection(editCookie, TF_IAS_QUERYONLY, nullptr, 0, &range);
    if (FAILED(result)) return result;

    ComPtr<ITfContextComposition> compositionContext;
    result = context->QueryInterface(IID_PPV_ARGS(&compositionContext));
    if (FAILED(result)) return result;
    result = compositionContext->StartComposition(editCookie, range.Get(), this, &composition_);
    if (SUCCEEDED(result) && composition_) {
        compositionContext_ = context;
    } else if (SUCCEEDED(result)) {
        // a read-only field refuses the composition with S_OK and no object
        result = E_FAIL;
    }
    return result;
}

void TextService::applyDisplayAttributes(TfEditCookie editCookie, ITfContext* context,
                                         ITfRange* range, const EngineResult& result) {
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
    candidateWindow_.hide();
    candidateActive_ = false;
    candidateAnchor_.Reset();
    candidateContext_.Reset();
}

bool TextService::requestCommitComposition(bool moveCaret) {
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
    resetCandidateState();
    if (engine_) engine_->reset();

    ComPtr<ITfComposition> oldComposition = composition_;
    ComPtr<ITfContext> oldContext = compositionContext_;
    composition_.Reset();
    compositionContext_.Reset();
    pendingCommitText_.clear();

    if (!oldComposition || !oldContext || clientId_ == TF_CLIENTID_NULL) return;
    auto* session = new (std::nothrow) TerminateEditSession(oldComposition.Get());
    if (!session) return;
    HRESULT editResult = E_FAIL;
    oldContext->RequestEditSession(clientId_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE,
                                   &editResult);
    session->Release();
}

HRESULT TextService::updateComposition(TfEditCookie editCookie, ITfContext* context,
                                       const EngineResult& result) {
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
    if (!secureMode_ && !result.notification.empty()) notificationWindow_.show(result.notification);
    return S_OK;
}

void TextService::updateCandidateWindow(TfEditCookie editCookie, ITfContext* context,
                                        const EngineResult& result) {
    const bool showCandidates = result.candidatesVisible && !result.candidates.empty();
    if (!showCandidates && result.message.empty()) {
        resetCandidateState();
        return;
    }

    ComPtr<ITfRange> range;
    if (composition_) composition_->GetRange(&range);
    if (!range) {
        TF_SELECTION selection{};
        ULONG fetched = 0;
        if (FAILED(context->GetSelection(editCookie, TF_DEFAULT_SELECTION, 1, &selection,
                                         &fetched)) ||
            !fetched) {
            resetCandidateState();
            return;
        }
        range.Attach(selection.range);
    }

    ComPtr<ITfContextView> view;
    RECT textRect{};
    BOOL clipped = FALSE;
    HWND owner = nullptr;
    if (FAILED(context->GetActiveView(&view)) ||
        FAILED(view->GetTextExt(editCookie, range.Get(), &textRect, &clipped)) ||
        FAILED(view->GetWnd(&owner))) {
        resetCandidateState();
        return;
    }
    if (showCandidates) {
        candidateWindow_.show(owner, textRect, result);
    } else {
        candidateWindow_.showMessage(owner, textRect, result.message);
    }
    candidateActive_ = showCandidates;
    candidateContext_ = context;
    candidateAnchor_.Reset();
    if (showCandidates && !composition_ && SUCCEEDED(range->Clone(&candidateAnchor_))) {
        candidateAnchor_->Collapse(editCookie, TF_ANCHOR_END);
    }
}

bool TextService::selectionMatchesTrackedState(TfEditCookie editCookie,
                                               ITfContext* context) const {
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

STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }

STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr* documentManager) {
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
    ComPtr<ITfContext> focusedContext;
    if (focused) focused->GetTop(&focusedContext);
    if (focusedContext.Get() != textEditContext_.Get()) punctuationKeyboard_.close();
    if ((composition_ || candidateActive_) &&
        textEditContext_.Get() != focusedContext.Get() && !pendingModeCommit_) {
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
    if ((composition_ || candidateActive_) && textEditContext_.Get() != context &&
        !pendingModeCommit_) {
        requestCommitComposition();
    }
    // a host without ITfSource still needs the push to succeed
    adviseTextEditSink(context);
    return S_OK;
}

STDMETHODIMP TextService::OnPopContext(ITfContext* context) {
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
    RefreshSettings();
    if (!symbolWindow_.isVisible() && ReadSymbolWindowState().visible) symbolWindow_.show();
    return S_OK;
}

STDMETHODIMP TextService::OnKillThreadFocus() {
    // Showing nonactivating IME UI can itself trigger this callback in some hosts.
    if (GetWindowThreadProcessId(GetForegroundWindow(), nullptr) != GetCurrentThreadId())
        punctuationKeyboard_.close();
    return S_OK;
}

STDMETHODIMP TextService::OnChange(REFGUID guid) {
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
    if (!items) return E_INVALIDARG;
    *items = new (std::nothrow) DisplayAttributeEnum();
    return *items ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guid,
                                                  ITfDisplayAttributeInfo** info) {
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
