#include "LangBarButton.h"

#include <algorithm>
#include <cwchar>
#include <new>

#include "ChiaKeyEngine.h"
#include "Guids.h"
#include "IconIds.h"
#include "ModuleState.h"
#include "TextService.h"

namespace ChiaKey::WindowsTsf {
namespace {

constexpr UINT kMenuToggleLanguage = 1;
constexpr UINT kMenuHalfWidth = 2;
constexpr UINT kMenuFullWidth = 3;
constexpr UINT kMenuSettings = 4;
constexpr UINT kMenuSymbols = 5;
constexpr wchar_t kSymbolsLabel[] = L"符號表（Ctrl+Alt+.）";
constexpr UINT kMenuPhraseEditor = 6;
constexpr UINT kMenuSimplifiedOutput = 7;
constexpr UINT kMenuAbout = 8;
constexpr UINT kMenuDictionary = 9;
constexpr wchar_t kSimplifiedOutputLabel[] = L"簡體輸出";
constexpr wchar_t kPhraseEditorLabel[] = L"詞彙編輯器…";
constexpr UINT kMenuFirstInputMethod = 100;

bool TaskbarIsLight() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value,
                        &size) == ERROR_SUCCESS &&
           value != 0;
}

// each icon ID is followed by its dark-taskbar variant
HICON LoadThemedIcon(int lightId) {
    const int id = TaskbarIsLight() ? lightId : lightId + 1;
    // the tray draws at the system DPI, whatever DPI the host app runs at
    const int size = GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem());
    return static_cast<HICON>(
        LoadImageW(g_module, MAKEINTRESOURCEW(id), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
}

// the selected one stays, so the menu always shows what is in use
std::vector<std::pair<std::string, std::wstring>> MenuInputMethods(const std::string& selected) {
    const std::vector<std::string> hidden = CurrentFrontendSettings().suppressedInputMethods;
    std::vector<std::pair<std::string, std::wstring>> result;
    for (auto& method : InputMethods()) {
        if (method.first == selected ||
            std::find(hidden.begin(), hidden.end(), method.first) == hidden.end()) {
            result.push_back(std::move(method));
        }
    }
    return result;
}

int ChineseIconFor(const std::string& inputMethod) {
    if (inputMethod == "SmartMandarin" || inputMethod == "TraditionalMandarin") return IDI_ZHUYIN;
    if (inputMethod == "Generic-cj-cin") return IDI_CANGJIE;
    if (inputMethod == "Generic-simplex-cin") return IDI_SIMPLEX;
    return IDI_CHINESE;
}

std::wstring InputMethodName(const std::string& inputMethod) {
    for (const auto& method : InputMethods()) {
        if (method.first == inputMethod) return method.second;
    }
    return UiText(L"中文");
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
    std::wstring value;
    if (kind_ == Kind::FullHalf) {
        value = service_->isFullWidthMode() ? UiText(L"全形（Shift+Space 切換）")
                                            : UiText(L"半形（Shift+Space 切換）");
    } else {
        const std::wstring name = InputMethodName(CurrentInputMethod());
        value = service_->isChineseMode() ? name + UiText(L"（按一下切換英文）")
                                          : UiText(L"英文（按一下切換") + name + UiText(L"）");
    }
    *tooltip = SysAllocString(value.c_str());
    return *tooltip ? S_OK : E_OUTOFMEMORY;
}

// both the right-click popup and the lang bar's ITfMenu show these items
std::vector<LangBarButton::MenuItem> LangBarButton::menuItems() {
    std::vector<MenuItem> items;
    items.push_back({kMenuToggleLanguage,
                     service_->isChineseMode() ? UiText(L"切換至英文") : UiText(L"切換至") + InputMethodName(CurrentInputMethod()), false});
    items.push_back({0, L"", false});
    const std::string selected = CurrentInputMethod();
    menuInputMethods_.clear();
    for (const auto& method : MenuInputMethods(selected)) {
        items.push_back({kMenuFirstInputMethod + static_cast<UINT>(menuInputMethods_.size()),
                         method.second, method.first == selected});
        menuInputMethods_.push_back(method.first);
    }
    items.push_back({0, L"", false});
    RefreshSettings();
    items.push_back({kMenuSimplifiedOutput, UiText(kSimplifiedOutputLabel),
                     CurrentFrontendSettings().simplifiedOutput});
    items.push_back({kMenuHalfWidth, UiText(L"半形"), !service_->isFullWidthMode()});
    items.push_back({kMenuFullWidth, UiText(L"全形"), service_->isFullWidthMode()});
    items.push_back({0, L"", false});
    items.push_back({kMenuSymbols, UiText(kSymbolsLabel), service_->isSymbolWindowVisible()});
    items.push_back({kMenuDictionary, UiText(L"字典…"), false});
    items.push_back({kMenuPhraseEditor, UiText(kPhraseEditorLabel), false});
    items.push_back({kMenuSettings, UiText(L"輸入法設定…"), false});
    items.push_back({kMenuAbout, UiText(L"關於…"), false});
    return items;
}

STDMETHODIMP LangBarButton::OnClick(TfLBIClick click, POINT point, const RECT*) {
    if (click == TF_LBI_CLK_RIGHT) {
        HMENU menu = CreatePopupMenu();
        if (!menu) return E_OUTOFMEMORY;
        for (const auto& item : menuItems()) {
            if (item.id == 0) {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            } else {
                AppendMenuW(menu, MF_STRING | (item.checked ? MF_CHECKED : 0), item.id,
                            item.label.c_str());
            }
        }
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
    for (const auto& item : menuItems()) {
        if (item.id == 0) {
            const HRESULT result = menu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr,
                                                     nullptr, 0, nullptr);
            if (FAILED(result)) return result;
            continue;
        }
        const HRESULT result = menu->AddMenuItem(
            item.id, item.checked ? TF_LBMENUF_CHECKED : 0, nullptr, nullptr,
            item.label.c_str(), static_cast<ULONG>(item.label.size()), nullptr);
        if (FAILED(result)) return result;
    }
    return S_OK;
}

STDMETHODIMP LangBarButton::OnMenuSelect(UINT id) {
    if (id == kMenuToggleLanguage) service_->toggleChineseMode();
    if (id == kMenuSimplifiedOutput && !service_->toggleSimplifiedOutput()) return E_FAIL;
    if (id == kMenuHalfWidth && service_->isFullWidthMode()) service_->toggleFullWidthMode();
    if (id == kMenuFullWidth && !service_->isFullWidthMode()) service_->toggleFullWidthMode();
    if (id == kMenuSettings) return service_->openSettings();
    if (id == kMenuPhraseEditor) return service_->openSettings(nullptr, L"/phrases");
    if (id == kMenuDictionary) return service_->openSettings(nullptr, L"/dictionary");
    if (id == kMenuAbout) return service_->openSettings(nullptr, L"/about");
    if (id == kMenuSymbols) service_->toggleSymbolWindow();
    if (id >= kMenuFirstInputMethod &&
        id - kMenuFirstInputMethod < menuInputMethods_.size()) {
        service_->selectInputMethod(menuInputMethods_[id - kMenuFirstInputMethod]);
    }
    return S_OK;
}

const wchar_t* LangBarButton::label() const {
    if (kind_ == Kind::FullHalf) return service_->isFullWidthMode() ? L"全" : L"半";
    return service_->isChineseMode() ? L"中" : L"英";
}

STDMETHODIMP LangBarButton::GetIcon(HICON* icon) {
    if (!icon) return E_INVALIDARG;
    int id = service_->isChineseMode() ? ChineseIconFor(CurrentInputMethod()) : IDI_ENGLISH;
    if (kind_ == Kind::FullHalf) id = service_->isFullWidthMode() ? IDI_FULL_WIDTH : IDI_HALF_WIDTH;
    *icon = LoadThemedIcon(id);
    return *icon ? S_OK : E_FAIL;
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
