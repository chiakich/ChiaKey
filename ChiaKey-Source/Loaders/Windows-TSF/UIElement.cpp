#include "UIElement.h"
#include "ModuleState.h"
#include <algorithm>
#include <oleauto.h>
#include <utility>

namespace ChiaKey::WindowsTsf {
TextUIElement::TextUIElement(bool candidates, GUID guid, std::wstring description)
    : candidates_(candidates), guid_(guid), description_(std::move(description)) { ++g_objectCount; }
TextUIElement::~TextUIElement() { --g_objectCount; }

STDMETHODIMP TextUIElement::QueryInterface(REFIID iid, void** object) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (iid == IID_IUnknown || iid == IID_ITfUIElement ||
        (candidates_ && iid == IID_ITfCandidateListUIElement)) {
        *object = static_cast<ITfCandidateListUIElement*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) TextUIElement::AddRef() { return ++references_; }
STDMETHODIMP_(ULONG) TextUIElement::Release() {
    ULONG remaining = --references_;
    if (!remaining) delete this;
    return remaining;
}
STDMETHODIMP TextUIElement::GetDescription(BSTR* description) {
    if (!description) return E_INVALIDARG;
    *description = SysAllocString(description_.c_str());
    return *description ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP TextUIElement::GetGUID(GUID* guid) {
    if (!guid) return E_INVALIDARG;
    *guid = guid_; return S_OK;
}
STDMETHODIMP TextUIElement::Show(BOOL show) {
    wantsWindow_ = show != FALSE;
    if (show_) show_(wantsWindow_);
    return S_OK;
}
STDMETHODIMP TextUIElement::IsShown(BOOL* shown) {
    if (!shown) return E_INVALIDARG;
    *shown = visible_ && visible_() ? TRUE : FALSE;
    return S_OK;
}
void TextUIElement::setCallbacks(std::function<void(bool)> show, std::function<bool()> visible) {
    show_ = std::move(show); visible_ = std::move(visible);
}
void TextUIElement::detach() {
    Show(FALSE); show_ = {}; visible_ = {}; document_.Reset();
}
void TextUIElement::update(ITfDocumentMgr* document, const std::vector<std::wstring>& strings,
                           UINT selection, UINT pageSize) {
    document_ = document;
    const bool rebuildPages = strings_ != strings || pageSize_ != pageSize;
    strings_ = strings;
    selection_ = strings_.empty() ? 0 : std::min(selection, static_cast<UINT>(strings_.size() - 1));
    if (!rebuildPages) return;
    pages_.clear();
    pageSize_ = pageSize;
    pageSize = std::max<UINT>(pageSize, 1);
    for (UINT start = 0; start < strings_.size(); start += pageSize) pages_.push_back(start);
}
STDMETHODIMP TextUIElement::GetUpdatedFlags(DWORD* flags) {
    if (!flags) return E_INVALIDARG;
    *flags = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION |
             TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
    return S_OK;
}
STDMETHODIMP TextUIElement::GetDocumentMgr(ITfDocumentMgr** document) {
    if (!document) return E_INVALIDARG;
    return document_.CopyTo(document);
}
STDMETHODIMP TextUIElement::GetCount(UINT* count) {
    if (!count) return E_INVALIDARG;
    *count = static_cast<UINT>(strings_.size()); return S_OK;
}
STDMETHODIMP TextUIElement::GetSelection(UINT* selection) {
    if (!selection) return E_INVALIDARG;
    *selection = selection_; return strings_.empty() ? S_FALSE : S_OK;
}
STDMETHODIMP TextUIElement::GetString(UINT index, BSTR* text) {
    if (!text) return E_INVALIDARG;
    *text = nullptr;
    if (index >= strings_.size()) return E_INVALIDARG;
    *text = SysAllocStringLen(strings_[index].data(), static_cast<UINT>(strings_[index].size()));
    return *text ? S_OK : E_OUTOFMEMORY;
}
STDMETHODIMP TextUIElement::GetPageIndex(UINT* indices, UINT size, UINT* count) {
    if (!count || (size && !indices)) return E_INVALIDARG;
    *count = static_cast<UINT>(pages_.size());
    std::copy_n(pages_.begin(), std::min(size, *count), indices);
    return size < *count ? S_FALSE : S_OK;
}
STDMETHODIMP TextUIElement::SetPageIndex(UINT* indices, UINT count) {
    if (!indices || !count || strings_.empty() || indices[0] != 0) return E_INVALIDARG;
    for (UINT i = 0; i < count; ++i) {
        if (indices[i] >= strings_.size() || (i && indices[i] <= indices[i-1])) return E_INVALIDARG;
    }
    pages_.assign(indices, indices + count); return S_OK;
}
STDMETHODIMP TextUIElement::GetCurrentPage(UINT* page) {
    if (!page) return E_INVALIDARG;
    *page = pages_.empty() ? 0 : static_cast<UINT>(
        std::upper_bound(pages_.begin(), pages_.end(), selection_) - pages_.begin() - 1);
    return S_OK;
}
HRESULT UIElementSession::present(ITfUIElementMgr* manager, TextUIElement* element, bool defaultShow) {
    if (!element) return E_INVALIDARG;
    if (element_.Get() != element || manager_.Get() != manager) {
        end(); element_ = element; manager_ = manager;
    }
    if (manager_ && !registered_) {
        BOOL show = defaultShow ? TRUE : FALSE;
        HRESULT result = manager_->BeginUIElement(element_.Get(), &show, &id_);
        if (FAILED(result)) { element_->Show(FALSE); return result; }
        registered_ = true;
        element_->Show(show);
        // Hosts often wait for UpdateUIElement before querying candidate strings.
        const HRESULT update = manager_->UpdateUIElement(id_);
        if (FAILED(update)) element_->Show(FALSE);
        return update;
    }
    if (manager_) {
        element_->Show(element_->wantsWindow());
        const HRESULT update = manager_->UpdateUIElement(id_);
        if (FAILED(update)) element_->Show(FALSE);
        return update;
    }
    return element_->Show(defaultShow ? TRUE : FALSE);
}
void UIElementSession::end() {
    auto element = std::move(element_);
    auto manager = std::move(manager_);
    const bool registered = registered_;
    registered_ = false;
    if (element) element->detach();
    if (manager && registered) manager->EndUIElement(id_);
}
} // namespace ChiaKey::WindowsTsf
