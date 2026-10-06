#include "ReloadableTextService.h"
#include "ModuleState.h"
#include "ReloadControl.h"
#include <ctffunc.h>
#include <wrl/client.h>
#include <new>
#include <mutex>

namespace ChiaKey::WindowsTsf {
namespace {
using Microsoft::WRL::ComPtr;

class ReloadableTextService final : public ITfTextInputProcessorEx,
                                    public ITfDisplayAttributeProvider,
                                    public ITfFunctionProvider,
                                    public ITfFnConfigure {
public:
    ReloadableTextService(TextServiceFactory latest, TextServiceRevision published)
        : latest_(latest), published_(published) { ++g_objectCount; }
    HRESULT initialize(TextServiceFactory fallback) {
        HRESULT hr = latest_(backend_.GetAddressOf(), &revision_);
        return FAILED(hr) ? fallback(backend_.ReleaseAndGetAddressOf(), &revision_) : hr;
    }
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfTextInputProcessor || iid == IID_ITfTextInputProcessorEx)
            *object = static_cast<ITfTextInputProcessorEx*>(this);
        else if (iid == IID_ITfDisplayAttributeProvider)
            *object = static_cast<ITfDisplayAttributeProvider*>(this);
        else if (iid == IID_ITfFunctionProvider)
            *object = static_cast<ITfFunctionProvider*>(this);
        else if (iid == IID_ITfFnConfigure || iid == IID_ITfFunction)
            *object = static_cast<ITfFnConfigure*>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP Activate(ITfThreadMgr* manager, TfClientId id) override {
        return ActivateEx(manager, id, 0);
    }
    STDMETHODIMP ActivateEx(ITfThreadMgr* manager, TfClientId id, DWORD flags) override {
        ReloadActivity operation(activity_);
        if (!manager || id == TF_CLIENTID_NULL) return E_INVALIDARG;
        if (switching_) return E_UNEXPECTED;
        if (active_) return S_OK;
        // Only replace the backend at a TSF activation boundary: never while
        // processing a key, an edit session, or an unfinished composition.
        ComPtr<ITfTextInputProcessorEx> next;
        std::wstring revision;
        if (SUCCEEDED(latest_(next.GetAddressOf(), &revision)) && next) {
            const HRESULT hr = next->ActivateEx(manager, id, flags);
            if (SUCCEEDED(hr)) {
                backend_ = next;
                revision_ = revision;
                active_ = true;
                activated(manager, id, flags);
                return hr;
            }
            next->Deactivate();
        }
        // A partially installed, incompatible or unavailable update must not
        // disable typing. Keep the last working backend until the next switch.
        const HRESULT hr = backend_->ActivateEx(manager, id, flags);
        active_ = SUCCEEDED(hr);
        if (active_) activated(manager, id, flags);
        return hr;
    }
    STDMETHODIMP Deactivate() override {
        ReloadActivity operation(activity_);
        if (switching_) { deactivateRequested_ = true; return S_OK; }
        if (!active_) return S_OK;
        const HRESULT hr = backend_->Deactivate();
        if (SUCCEEDED(hr)) {
            active_ = false;
            recovering_ = false;
            stopMonitor();
            manager_.Reset();
        }
        return hr;
    }
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** items) override {
        ReloadActivity operation(activity_);
        ComPtr<ITfDisplayAttributeProvider> provider;
        const HRESULT hr = backend_.As(&provider);
        return FAILED(hr) ? hr : provider->EnumDisplayAttributeInfo(items);
    }
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info) override {
        ReloadActivity operation(activity_);
        ComPtr<ITfDisplayAttributeProvider> provider;
        const HRESULT hr = backend_.As(&provider);
        return FAILED(hr) ? hr : provider->GetDisplayAttributeInfo(guid, info);
    }
    STDMETHODIMP GetType(GUID* guid) override {
        ReloadActivity operation(activity_);
        ComPtr<ITfFunctionProvider> provider;
        const HRESULT hr = backend_.As(&provider);
        return FAILED(hr) ? hr : provider->GetType(guid);
    }
    STDMETHODIMP GetDescription(BSTR* description) override {
        ReloadActivity operation(activity_);
        ComPtr<ITfFunctionProvider> provider;
        const HRESULT hr = backend_.As(&provider);
        return FAILED(hr) ? hr : provider->GetDescription(description);
    }
    STDMETHODIMP GetFunction(REFGUID guid, REFIID iid, IUnknown** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (guid != GUID_NULL || iid != IID_ITfFnConfigure) return E_NOINTERFACE;
        return QueryInterface(iid, reinterpret_cast<void**>(object));
    }
    STDMETHODIMP GetDisplayName(BSTR* name) override {
        ReloadActivity operation(activity_);
        ComPtr<ITfFnConfigure> configure;
        const HRESULT hr = backend_.As(&configure);
        return FAILED(hr) ? hr : configure->GetDisplayName(name);
    }
    STDMETHODIMP Show(HWND parent, LANGID language, REFGUID profile) override {
        ReloadActivity operation(activity_);
        ComPtr<ITfFnConfigure> configure;
        const HRESULT hr = backend_.As(&configure);
        return FAILED(hr) ? hr : configure->Show(parent, language, profile);
    }
private:
    static LRESULT CALLBACK MonitorProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto* service = reinterpret_cast<ReloadableTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            service = static_cast<ReloadableTextService*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(service));
        }
        if (message == WM_TIMER && wp == 1 && service) {
            service->AddRef();
            service->pollUpdate();
            service->Release();
            return 0;
        }
        if (message == WM_NCDESTROY) SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return DefWindowProcW(window, message, wp, lp);
    }
    void activated(ITfThreadMgr* manager, TfClientId id, DWORD flags) {
        manager_ = manager;
        clientId_ = id;
        flags_ = flags;
        recovering_ = false;
        retryAfter_ = 0;
        if (monitor_) return;
        static std::once_flag once;
        static bool registered = false;
        std::call_once(once, [] {
            HMODULE pinned = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                   reinterpret_cast<LPCWSTR>(g_module), &pinned)) return;
            WNDCLASSW cls{};
            cls.hInstance = g_module;
            cls.lpfnWndProc = MonitorProc;
            cls.lpszClassName = L"ChiaKey.ReloadMonitor.V1";
            registered = RegisterClassW(&cls) != 0;
        });
        if (!registered) return; // manual deactivate/activate remains available
        monitor_ = CreateWindowExW(0, L"ChiaKey.ReloadMonitor.V1", L"", 0, 0, 0, 0, 0,
                                   HWND_MESSAGE, nullptr, g_module, this);
        if (monitor_ && !SetTimer(monitor_, 1, 1000, nullptr)) stopMonitor();
    }
    void stopMonitor() {
        if (!monitor_) return;
        KillTimer(monitor_, 1);
        DestroyWindow(monitor_);
        monitor_ = nullptr;
    }
    void pollUpdate() {
        if (!active_ || activity_ || switching_ || GetTickCount64() < retryAfter_) return;
        ReloadActivity operation(activity_);
        if (recovering_) {
            retryAfter_ = GetTickCount64() + 5000;
            switching_ = true;
            ComPtr<IChiaKeyReloadControl> control;
            if (SUCCEEDED(backend_->ActivateEx(manager_.Get(), clientId_, flags_)) &&
                SUCCEEDED(backend_.As(&control)) &&
                SUCCEEDED(control->RestoreReloadState(recoveryState_))) recovering_ = false;
            switching_ = false;
            if (deactivateRequested_) { deactivateRequested_ = false; Deactivate(); }
            return;
        }
        const auto published = published_();
        if (published.empty() || _wcsicmp(published.c_str(), revision_.c_str()) == 0) return;
        ComPtr<IChiaKeyReloadControl> current;
        DWORD state = 0;
        if (FAILED(backend_.As(&current)) || current->GetReloadState(&state) != S_OK) return;
        // WM_TIMER runs on the activating STA, at low priority. The backend
        // also rejects nested callbacks, queued edits, composition and held keys.
        switching_ = true;
        retryAfter_ = GetTickCount64() + 5000;
        ComPtr<ITfTextInputProcessorEx> next;
        ComPtr<IChiaKeyReloadControl> control;
        std::wstring revision;
        if (SUCCEEDED(latest_(next.GetAddressOf(), &revision)) && next &&
            SUCCEEDED(next.As(&control)) && !deactivateRequested_ &&
            current->GetReloadState(&state) == S_OK && SUCCEEDED(backend_->Deactivate()) &&
            !deactivateRequested_) {
            HRESULT hr = next->ActivateEx(manager_.Get(), clientId_, flags_);
            if (SUCCEEDED(hr) && !deactivateRequested_) hr = control->RestoreReloadState(state);
            if (deactivateRequested_) {
                next->Deactivate();
            } else if (SUCCEEDED(hr)) {
                backend_ = next;
                revision_ = revision;
                retryAfter_ = 0;
            } else {
                next->Deactivate();
                const HRESULT restored = backend_->ActivateEx(manager_.Get(), clientId_, flags_);
                recovering_ = FAILED(restored) || FAILED(current->RestoreReloadState(state));
                recoveryState_ = state;
                // Even if reactivation failed, retain the old backend and timer
                // so a subsequent update attempt can recover on the same STA.
            }
        }
        switching_ = false;
        if (deactivateRequested_) {
            deactivateRequested_ = false;
            Deactivate();
        }
    }
    ~ReloadableTextService() { stopMonitor(); backend_.Reset(); --g_objectCount; }
    std::atomic<ULONG> references_{1};
    TextServiceFactory latest_;
    TextServiceRevision published_;
    std::wstring revision_;
    ComPtr<ITfTextInputProcessorEx> backend_;
    ComPtr<ITfThreadMgr> manager_;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    DWORD flags_ = 0;
    HWND monitor_ = nullptr;
    unsigned activity_ = 0;
    ULONGLONG retryAfter_ = 0;
    bool switching_ = false;
    bool deactivateRequested_ = false;
    bool recovering_ = false;
    DWORD recoveryState_ = 0;
    bool active_ = false;
};
} // namespace

HRESULT CreateReloadableTextService(IUnknown* outer, REFIID iid, void** object,
                                   TextServiceFactory latest, TextServiceFactory fallback,
                                   TextServiceRevision publishedRevision) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (outer) return CLASS_E_NOAGGREGATION;
    auto* service = new (std::nothrow) ReloadableTextService(latest, publishedRevision);
    if (!service) return E_OUTOFMEMORY;
    HRESULT hr = service->initialize(fallback);
    if (SUCCEEDED(hr)) hr = service->QueryInterface(iid, object);
    service->Release();
    return hr;
}
} // namespace ChiaKey::WindowsTsf
