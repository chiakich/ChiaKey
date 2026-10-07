#include "TextService.h"
#include "ModuleState.h"
#include <iostream>
#include <stdexcept>
namespace ChiaKey::WindowsTsf {
HMODULE g_module = nullptr;
std::atomic<long> g_objectCount{0}, g_serverLocks{0};
namespace {
using Microsoft::WRL::ComPtr;
void Check(bool pass, const char* message) {
    if (!pass) throw std::runtime_error(message);
    std::cout << "PASS: " << message << '\n';
}
class Context final : public ITfContext, public ITfSource {
    ULONG refs_ = 1;
public:
    unsigned requests = 0;
    STDMETHODIMP QueryInterface(REFIID iid, void** p) override {
        *p = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfContext) *p = static_cast<ITfContext*>(this);
        else if (iid == IID_ITfSource) *p = static_cast<ITfSource*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override { ULONG n = --refs_; if (!n) delete this; return n; }
    STDMETHODIMP RequestEditSession(TfClientId, ITfEditSession*, DWORD, HRESULT* result) override { ++requests; *result = TF_E_READONLY; return S_OK; }
    STDMETHODIMP InWriteSession(TfClientId, BOOL*) override { return E_NOTIMPL; }
    STDMETHODIMP GetSelection(TfEditCookie, ULONG, ULONG, TF_SELECTION*, ULONG*) override { return E_NOTIMPL; }
    STDMETHODIMP SetSelection(TfEditCookie, ULONG, const TF_SELECTION*) override { return E_NOTIMPL; }
    STDMETHODIMP GetStart(TfEditCookie, ITfRange**) override { return E_NOTIMPL; }
    STDMETHODIMP GetEnd(TfEditCookie, ITfRange**) override { return E_NOTIMPL; }
    STDMETHODIMP GetActiveView(ITfContextView**) override { return E_NOTIMPL; }
    STDMETHODIMP EnumViews(IEnumTfContextViews**) override { return E_NOTIMPL; }
    STDMETHODIMP GetStatus(TF_STATUS*) override { return E_NOTIMPL; }
    STDMETHODIMP GetProperty(REFGUID, ITfProperty**) override { return E_NOTIMPL; }
    STDMETHODIMP GetAppProperty(REFGUID, ITfReadOnlyProperty**) override { return E_NOTIMPL; }
    STDMETHODIMP TrackProperties(const GUID**, ULONG, const GUID**, ULONG, ITfReadOnlyProperty**) override { return E_NOTIMPL; }
    STDMETHODIMP EnumProperties(IEnumTfProperties**) override { return E_NOTIMPL; }
    STDMETHODIMP GetDocumentMgr(ITfDocumentMgr**) override { return E_NOTIMPL; }
    STDMETHODIMP CreateRangeBackup(TfEditCookie, ITfRange*, ITfRangeBackup**) override { return E_NOTIMPL; }
    STDMETHODIMP AdviseSink(REFIID, IUnknown*, DWORD* cookie) override { *cookie = 1; return S_OK; }
    STDMETHODIMP UnadviseSink(DWORD) override { return S_OK; }
};
class Document final : public ITfDocumentMgr {
    ULONG refs_ = 1;
public:
    ComPtr<ITfContext> context;
    STDMETHODIMP QueryInterface(REFIID iid, void** p) override {
        *p = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfDocumentMgr) *p = static_cast<ITfDocumentMgr*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override { ULONG n = --refs_; if (!n) delete this; return n; }
    STDMETHODIMP CreateContext(TfClientId, DWORD, IUnknown*, ITfContext**, TfEditCookie*) override { return E_NOTIMPL; }
    STDMETHODIMP Push(ITfContext*) override { return E_NOTIMPL; }
    STDMETHODIMP Pop(DWORD) override { return E_NOTIMPL; }
    STDMETHODIMP GetTop(ITfContext** p) override { return context.CopyTo(p); }
    STDMETHODIMP GetBase(ITfContext** p) override { return context.CopyTo(p); }
    STDMETHODIMP EnumContexts(IEnumTfContexts**) override { return E_NOTIMPL; }
};
class Manager final : public ITfThreadMgr {
    ULONG refs_ = 1;
public:
    ComPtr<ITfDocumentMgr> focused;
    STDMETHODIMP QueryInterface(REFIID iid, void** p) override {
        *p = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfThreadMgr) *p = static_cast<ITfThreadMgr*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override { ULONG n = --refs_; if (!n) delete this; return n; }
    STDMETHODIMP Activate(TfClientId*) override { return E_NOTIMPL; }
    STDMETHODIMP Deactivate() override { return E_NOTIMPL; }
    STDMETHODIMP CreateDocumentMgr(ITfDocumentMgr**) override { return E_NOTIMPL; }
    STDMETHODIMP EnumDocumentMgrs(IEnumTfDocumentMgrs**) override { return E_NOTIMPL; }
    STDMETHODIMP GetFocus(ITfDocumentMgr** p) override { return focused.CopyTo(p); }
    STDMETHODIMP SetFocus(ITfDocumentMgr*) override { return E_NOTIMPL; }
    STDMETHODIMP AssociateFocus(HWND, ITfDocumentMgr*, ITfDocumentMgr**) override { return E_NOTIMPL; }
    STDMETHODIMP IsThreadFocus(BOOL*) override { return E_NOTIMPL; }
    STDMETHODIMP GetFunctionProvider(REFCLSID, ITfFunctionProvider**) override { return E_NOTIMPL; }
    STDMETHODIMP EnumFunctionProviders(IEnumTfFunctionProviders**) override { return E_NOTIMPL; }
    STDMETHODIMP GetGlobalCompartment(ITfCompartmentMgr**) override { return E_NOTIMPL; }
};
class Composition final : public ITfComposition {
    ULONG refs_ = 1;
public:
    
    STDMETHODIMP QueryInterface(REFIID iid, void** p) override {
        *p = nullptr;
        if (iid == IID_IUnknown || iid == IID_ITfComposition) *p = static_cast<ITfComposition*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override { ULONG n = --refs_; if (!n) delete this; return n; }
    STDMETHODIMP GetRange(ITfRange**) override { return E_NOTIMPL; }
    STDMETHODIMP ShiftStart(TfEditCookie, ITfRange*) override { return E_NOTIMPL; }
    STDMETHODIMP ShiftEnd(TfEditCookie, ITfRange*) override { return E_NOTIMPL; }
    STDMETHODIMP EndComposition(TfEditCookie) override { return S_OK; }
};
}
// Exercise production sinks with COM host doubles, without registration,
// windows, lexicon, user settings, or loading a test DLL into Office.
struct TextServiceFocusTest {
    static void Run() {
        ComPtr<Context> oldContext, composing, other;
        oldContext.Attach(new Context); composing.Attach(new Context); other.Attach(new Context);
        ComPtr<Document> active, different;
        active.Attach(new Document); active->context = composing;
        different.Attach(new Document); different->context = other;
        ComPtr<Manager> manager; manager.Attach(new Manager);
        ComPtr<TextService> service; service.Attach(new TextService);
        service->threadManager_ = manager; service->clientId_ = 1;
        auto seed = [&] {
            service->composition_.Attach(new Composition);
            service->compositionContext_ = composing;
            service->adviseTextEditSink(oldContext.Get());
            manager->focused = active;
        };
        seed();
        service->OnSetFocus(active.Get(), nullptr);
        Check(composing->requests == 0 && service->composition_,
              "composition context survives focus restoration even when the old sink differs");
        Check(service->textEditContext_.Get() == composing.Get(), "sink reconnects to restored context");
        seed();
        service->keyEditDepth_ = 2;
        service->OnSetFocus(nullptr, active.Get());
        service->OnSetFocus(active.Get(), nullptr);
        service->finishKeyEdit();
        Check(service->deferredDocumentFocus_ && composing->requests == 0,
              "nested edit does not replay transient null focus");
        service->finishKeyEdit();
        Check(!service->deferredDocumentFocus_ && service->composition_ && composing->requests == 0,
              "queued edit reconciles restored focus without committing");
        seed();
        service->keyEditDepth_ = 1; service->keyDownVirtualKey_ = 'S';
        service->OnSetFocus(nullptr, active.Get());
        service->OnSetFocus(active.Get(), nullptr);
        service->finishKeyEdit();
        Check(service->deferredDocumentFocus_, "defer focus until the physical key release");
        BOOL eaten = TRUE;
        service->OnTestKeyUp(composing.Get(), 'S', 0, &eaten);
        Check(!eaten && !service->deferredDocumentFocus_ && service->composition_ && composing->requests == 0,
              "unclaimed key-up test still reconciles final focus");
        seed();
        service->keyEditDepth_ = 1;
        service->OnSetFocus(different.Get(), active.Get()); manager->focused = different;
        service->finishKeyEdit();
        Check(composing->requests == 1, "genuine document switch still requests commit");
        Check(service->textEditContext_.Get() == other.Get(), "genuine switch reconnects sink");
        seed();
        service->keyEditDepth_ = 1;
        service->OnSetFocus(nullptr, active.Get()); manager->focused.Reset();
        service->finishKeyEdit();
        Check(composing->requests == 2 && !service->textEditContext_,
              "genuine loss of focus still commits and disconnects sink");
        service->composition_.Reset(); service->compositionContext_.Reset();
        service->threadManager_.Reset();
    }
};
}
int main() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 0;
    try { ChiaKey::WindowsTsf::TextServiceFocusTest::Run(); }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; result = 1; }
    CoUninitialize(); return result;
}


