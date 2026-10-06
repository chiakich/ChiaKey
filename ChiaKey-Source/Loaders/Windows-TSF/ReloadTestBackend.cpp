// A real DLL fixture: no TSF registration, windows, lexicon or user data.
#include <Windows.h>
#include <msctf.h>
#include <atomic>
#include <new>
#include "ReloadControl.h"
using namespace ChiaKey::WindowsTsf;

namespace {
bool failActivation = false;
bool busy = false, failRestore = false;
DWORD inputState = kReloadChinese;
int activeCount = 0;
int activationAttempts = 0;
ITfTextInputProcessor* deactivateTarget = nullptr;
class Backend final : public ITfTextInputProcessorEx, public ITfFunctionProvider,
                      public IChiaKeyReloadControl {
public:
    STDMETHODIMP QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_INVALIDARG;
        *out = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfTextInputProcessor || iid == IID_ITfTextInputProcessorEx)
            *out = static_cast<ITfTextInputProcessorEx*>(this);
        else if (iid == IID_ITfFunctionProvider) *out = static_cast<ITfFunctionProvider*>(this);
        else if (iid == __uuidof(IChiaKeyReloadControl)) *out = static_cast<IChiaKeyReloadControl*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG n = --refs_; if (!n) delete this; return n;
    }
    STDMETHODIMP Activate(ITfThreadMgr* m, TfClientId id) override { return ActivateEx(m, id, 0); }
    STDMETHODIMP ActivateEx(ITfThreadMgr*, TfClientId, DWORD flags) override {
        ++activationAttempts;
        flags_ = flags;
        if (failActivation) return E_FAIL;
        if (!active_) { active_ = true; ++activeCount; inputState = kReloadChinese; }
        if (deactivateTarget) {
            auto* target = deactivateTarget;
            deactivateTarget = nullptr;
            target->Deactivate();
        }
        return S_OK;
    }
    STDMETHODIMP Deactivate() override {
        if (active_) { active_ = false; --activeCount; }
        return S_OK;
    }
    STDMETHODIMP GetReloadState(DWORD* state) override {
        if (!state) return E_INVALIDARG;
        *state = inputState;
        return busy ? S_FALSE : S_OK;
    }
    STDMETHODIMP RestoreReloadState(DWORD state) override {
        if (failRestore) return E_FAIL;
        inputState = state; return S_OK;
    }
    STDMETHODIMP GetType(GUID* guid) override {
        if (!guid) return E_INVALIDARG;
        *guid = GUID_NULL; guid->Data1 = TEST_BACKEND_VERSION; guid->Data2 = static_cast<WORD>(flags_);
        guid->Data3 = static_cast<WORD>(inputState);
        return S_OK;
    }
    STDMETHODIMP GetDescription(BSTR* out) override {
        if (!out) return E_INVALIDARG;
        *out = SysAllocString(L"reload fixture"); return *out ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetFunction(REFGUID, REFIID, IUnknown** out) override {
        if (!out) return E_INVALIDARG;
        *out = nullptr; return E_NOINTERFACE;
    }
private:
    std::atomic<ULONG> refs_{1};
    DWORD flags_ = 0;
    bool active_ = false;
};
}
extern "C" HRESULT WINAPI ChiaKeyCreateTextServiceV1(ITfTextInputProcessorEx** out) {
    if (!out) return E_INVALIDARG;
    *out = nullptr;
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&ChiaKeyCreateTextServiceV1), &pinned)) return E_FAIL;
    *out = new (std::nothrow) Backend;
    return *out ? S_OK : E_OUTOFMEMORY;
}
extern "C" void WINAPI FailActivation(BOOL fail) { failActivation = fail != FALSE; }
extern "C" void WINAPI SetBusy(BOOL value) { busy = value != FALSE; }
extern "C" void WINAPI FailRestore(BOOL value) { failRestore = value != FALSE; }
extern "C" void WINAPI SetInputState(DWORD state) { inputState = state; }
extern "C" int WINAPI ActiveCount() { return activeCount; }
extern "C" int WINAPI ActivationAttempts() { return activationAttempts; }
extern "C" void WINAPI DeactivateDuringActivation(ITfTextInputProcessor* target) { deactivateTarget = target; }
