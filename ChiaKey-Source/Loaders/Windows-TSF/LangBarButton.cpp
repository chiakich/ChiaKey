#include "LangBarButton.h"

#include <algorithm>
#include <cwchar>
#include <new>

#include "ChiaKeyEngine.h"
#include "Guids.h"
#include "TextService.h"

namespace ChiaKey::WindowsTsf {
namespace {

constexpr UINT kMenuToggleLanguage = 1;
constexpr UINT kMenuHalfWidth = 2;
constexpr UINT kMenuFullWidth = 3;
constexpr UINT kMenuFirstInputMethod = 100;

HICON CreateLabelIcon(const wchar_t* label, COLORREF background) {
    HDC screen = GetDC(nullptr);
    if (!screen) return nullptr;
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, 16, 16);
    HBITMAP mask = CreateBitmap(16, 16, 1, 1, nullptr);
    if (!memory || !color || !mask) {
        if (mask) DeleteObject(mask);
        if (color) DeleteObject(color);
        if (memory) DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        return nullptr;
    }

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memory, color));
    RECT rectangle{0, 0, 16, 16};
    HBRUSH backgroundBrush = CreateSolidBrush(background);
    FillRect(memory, &rectangle, backgroundBrush);
    DeleteObject(backgroundBrush);
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(32, 33, 36));
    const int fontHeight = wcslen(label) == 1 ? -17 : -13;
    HFONT font = CreateFontW(fontHeight, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                             L"Microsoft JhengHei UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(memory, font));
    DrawTextW(memory, label, -1, &rectangle,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(memory, oldFont);

    // opaque, since Windows 11 does not always recolor a third-party TIP icon
    HDC maskDc = CreateCompatibleDC(screen);
    HBITMAP oldMask = static_cast<HBITMAP>(SelectObject(maskDc, mask));
    PatBlt(maskDc, 0, 0, 16, 16, BLACKNESS);
    SelectObject(maskDc, oldMask);
    DeleteDC(maskDc);
    SelectObject(memory, oldBitmap);

    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    HICON result = CreateIconIndirect(&info);

    DeleteObject(font);
    DeleteObject(mask);
    DeleteObject(color);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return result;
}

}  // namespace

LangBarButton::LangBarButton(TextService* service, REFGUID guid, Kind kind)
    : service_(service), guid_(guid), kind_(kind) {
    if (service_) service_->AddRef();
}

LangBarButton::~LangBarButton() {
    std::vector<std::pair<DWORD, ITfLangBarItemSink*>> sinks;
    {
        std::lock_guard<std::mutex> lock(sinksMutex_);
        sinks.swap(sinks_);
    }
    for (auto& entry : sinks) {
        if (entry.second) entry.second->Release();
    }
    if (service_) service_->Release();
}

STDMETHODIMP LangBarButton::QueryInterface(REFIID iid, void** object) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (iid == IID_IUnknown || iid == IID_ITfLangBarItem || iid == IID_ITfLangBarItemButton) {
        *object = static_cast<ITfLangBarItemButton*>(this);
    } else if (iid == IID_ITfSource) {
        *object = static_cast<ITfSource*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) LangBarButton::AddRef() { return ++referenceCount_; }
STDMETHODIMP_(ULONG) LangBarButton::Release() {
    const ULONG remaining = --referenceCount_;
    if (!remaining) delete this;
    return remaining;
}

STDMETHODIMP LangBarButton::GetInfo(TF_LANGBARITEMINFO* info) {
    if (!info) return E_INVALIDARG;
    info->clsidService = kTextServiceClsid;
    info->guidItem = guid_;
    info->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
    info->ulSort = kind_ == Kind::FullHalf ? 1 : 0;
    wcscpy_s(info->szDescription, kTextServiceDescription);
    return S_OK;
}

STDMETHODIMP LangBarButton::GetStatus(DWORD* status) {
    if (!status) return E_INVALIDARG;
    *status = 0;
    return S_OK;
}

STDMETHODIMP LangBarButton::Show(BOOL) { return E_NOTIMPL; }

STDMETHODIMP LangBarButton::GetTooltipString(BSTR* tooltip) {
    if (!tooltip) return E_INVALIDARG;
    const wchar_t* value = kTextServiceDescription;
    if (kind_ == Kind::FullHalf) {
        value = service_->isFullWidthMode() ? L"全形（Shift+Space 切換）"
                                            : L"半形（Shift+Space 切換）";
    } else {
        value = service_->isChineseMode() ? L"中文注音（按一下切換英文）"
                                          : L"英文（按一下切換中文注音）";
    }
    *tooltip = SysAllocString(value);
    return *tooltip ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarButton::OnClick(TfLBIClick click, POINT point, const RECT*) {
    if (click == TF_LBI_CLK_RIGHT) {
        HMENU menu = CreatePopupMenu();
        if (!menu) return E_OUTOFMEMORY;
        AppendMenuW(menu, MF_STRING, kMenuToggleLanguage,
                    service_->isChineseMode() ? L"切換至英文" : L"切換至中文注音");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        const std::string selected = CurrentInputMethod();
        const auto methods = InputMethods();
        menuInputMethods_.clear();
        for (const auto& method : methods) {
            AppendMenuW(menu, MF_STRING | (method.first == selected ? MF_CHECKED : 0),
                        kMenuFirstInputMethod + static_cast<UINT>(menuInputMethods_.size()),
                        method.second.c_str());
            menuInputMethods_.push_back(method.first);
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (!service_->isFullWidthMode() ? MF_CHECKED : 0),
                    kMenuHalfWidth, L"半形");
        AppendMenuW(menu, MF_STRING | (service_->isFullWidthMode() ? MF_CHECKED : 0),
                    kMenuFullWidth, L"全形");
        HWND owner = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 0, 0, HWND_DESKTOP,
                                     nullptr, nullptr, nullptr);
        const UINT chosen = TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_BOTTOMALIGN, point.x,
            point.y, 0, owner ? owner : GetDesktopWindow(), nullptr);
        if (chosen) OnMenuSelect(chosen);
        if (owner) DestroyWindow(owner);
        DestroyMenu(menu);
        return S_OK;
    }
    if (click != TF_LBI_CLK_LEFT) return S_OK;
    if (kind_ == Kind::FullHalf) {
        service_->toggleFullWidthMode();
    } else {
        service_->toggleChineseMode();
    }
    return S_OK;
}

STDMETHODIMP LangBarButton::InitMenu(ITfMenu* menu) {
    if (!menu) return E_INVALIDARG;
    HRESULT result = menu->AddMenuItem(
        kMenuToggleLanguage, 0, nullptr, nullptr,
        service_->isChineseMode() ? L"切換至英文" : L"切換至中文注音",
        service_->isChineseMode() ? 5 : 7, nullptr);
    if (FAILED(result)) return result;
    result = menu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);
    if (FAILED(result)) return result;
    const std::string selected = CurrentInputMethod();
    menuInputMethods_.clear();
    for (const auto& method : InputMethods()) {
        result = menu->AddMenuItem(
            kMenuFirstInputMethod + static_cast<UINT>(menuInputMethods_.size()),
            method.first == selected ? TF_LBMENUF_CHECKED : 0, nullptr, nullptr,
            method.second.c_str(), static_cast<ULONG>(method.second.size()), nullptr);
        if (FAILED(result)) return result;
        menuInputMethods_.push_back(method.first);
    }
    result = menu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);
    if (FAILED(result)) return result;
    result = menu->AddMenuItem(kMenuHalfWidth,
                               service_->isFullWidthMode() ? 0 : TF_LBMENUF_CHECKED, nullptr,
                               nullptr, L"半形", 2, nullptr);
    if (FAILED(result)) return result;
    return menu->AddMenuItem(kMenuFullWidth,
                             service_->isFullWidthMode() ? TF_LBMENUF_CHECKED : 0, nullptr,
                             nullptr, L"全形", 2, nullptr);
}

STDMETHODIMP LangBarButton::OnMenuSelect(UINT id) {
    if (id == kMenuToggleLanguage) service_->toggleChineseMode();
    if (id == kMenuHalfWidth && service_->isFullWidthMode()) service_->toggleFullWidthMode();
    if (id == kMenuFullWidth && !service_->isFullWidthMode()) service_->toggleFullWidthMode();
    if (id >= kMenuFirstInputMethod &&
        id - kMenuFirstInputMethod < menuInputMethods_.size()) {
        service_->selectInputMethod(menuInputMethods_[id - kMenuFirstInputMethod]);
    }
    return S_OK;
}

const wchar_t* LangBarButton::label() const {
    if (kind_ == Kind::FullHalf) return service_->isFullWidthMode() ? L"全" : L"半";
    return service_->isChineseMode() ? L"ㄅ" : L"英";
}

STDMETHODIMP LangBarButton::GetIcon(HICON* icon) {
    if (!icon) return E_INVALIDARG;
    *icon = CreateLabelIcon(label(), RGB(255, 255, 255));
    return *icon ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarButton::GetText(BSTR* text) {
    if (!text) return E_INVALIDARG;
    *text = SysAllocString(label());
    return *text ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarButton::AdviseSink(REFIID iid, IUnknown* unknown, DWORD* cookie) {
    if (!unknown || !cookie) return E_INVALIDARG;
    *cookie = TF_INVALID_COOKIE;
    if (iid != IID_ITfLangBarItemSink) return E_NOINTERFACE;
    ITfLangBarItemSink* sink = nullptr;
    if (FAILED(unknown->QueryInterface(IID_PPV_ARGS(&sink)))) return E_NOINTERFACE;
    try {
        std::lock_guard<std::mutex> lock(sinksMutex_);
        *cookie = nextCookie_++;
        sinks_.emplace_back(*cookie, sink);
    } catch (const std::bad_alloc&) {
        sink->Release();
        *cookie = TF_INVALID_COOKIE;
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

STDMETHODIMP LangBarButton::UnadviseSink(DWORD cookie) {
    ITfLangBarItemSink* sink = nullptr;
    {
        std::lock_guard<std::mutex> lock(sinksMutex_);
        const auto found = std::find_if(sinks_.begin(), sinks_.end(),
                                        [cookie](const auto& entry) {
                                            return entry.first == cookie;
                                        });
        if (found == sinks_.end()) return E_INVALIDARG;
        sink = found->second;
        sinks_.erase(found);
    }
    sink->Release();
    return S_OK;
}

void LangBarButton::update() {
    std::vector<ITfLangBarItemSink*> snapshot;
    try {
        std::lock_guard<std::mutex> lock(sinksMutex_);
        snapshot.reserve(sinks_.size());
        for (const auto& entry : sinks_) {
            snapshot.push_back(entry.second);
            entry.second->AddRef();
        }
    } catch (const std::bad_alloc&) {
        for (ITfLangBarItemSink* sink : snapshot) sink->Release();
        return;
    }
    for (ITfLangBarItemSink* sink : snapshot) {
        sink->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP);
        sink->Release();
    }
}

}  // namespace ChiaKey::WindowsTsf
