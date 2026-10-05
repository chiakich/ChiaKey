#pragma once

#include <Windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace ChiaKey::WindowsTsf {

// A host can keep this COM object after the TIP has stopped presenting it.
// Detaching the callbacks prevents such objects from accessing a dead TIP.
class TextUIElement final : public ITfCandidateListUIElement {
public:
    TextUIElement(bool candidates, GUID guid, std::wstring description);
    ~TextUIElement();
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP GetDescription(BSTR* description) override;
    STDMETHODIMP GetGUID(GUID* guid) override;
    STDMETHODIMP Show(BOOL show) override;
    STDMETHODIMP IsShown(BOOL* shown) override;
    STDMETHODIMP GetUpdatedFlags(DWORD* flags) override;
    STDMETHODIMP GetDocumentMgr(ITfDocumentMgr** document) override;
    STDMETHODIMP GetCount(UINT* count) override;
    STDMETHODIMP GetSelection(UINT* selection) override;
    STDMETHODIMP GetString(UINT index, BSTR* text) override;
    STDMETHODIMP GetPageIndex(UINT* indices, UINT size, UINT* count) override;
    STDMETHODIMP SetPageIndex(UINT* indices, UINT count) override;
    STDMETHODIMP GetCurrentPage(UINT* page) override;

    void setCallbacks(std::function<void(bool)> show, std::function<bool()> visible);
    void detach();
    void update(ITfDocumentMgr* document, const std::vector<std::wstring>& strings,
                UINT selection, UINT pageSize);
    bool wantsWindow() const { return wantsWindow_; }

private:
    std::atomic<ULONG> references_{1};
    bool candidates_;
    GUID guid_;
    std::wstring description_;
    bool wantsWindow_ = false;
    std::function<void(bool)> show_;
    std::function<bool()> visible_;
    Microsoft::WRL::ComPtr<ITfDocumentMgr> document_;
    std::vector<std::wstring> strings_;
    std::vector<UINT> pages_;
    UINT selection_ = 0;
    UINT pageSize_ = 0;
};

// Begin/Update/End apply to both local windows and host-rendered candidates.
class UIElementSession final {
public:
    ~UIElementSession() { end(); }
    HRESULT present(ITfUIElementMgr* manager, TextUIElement* element, bool defaultShow);
    void end();
    TextUIElement* element() const { return element_.Get(); }
private:
    Microsoft::WRL::ComPtr<ITfUIElementMgr> manager_;
    Microsoft::WRL::ComPtr<TextUIElement> element_;
    DWORD id_ = 0;
    bool registered_ = false;
};

} // namespace ChiaKey::WindowsTsf
