#pragma once

#include <Windows.h>
#include <msctf.h>

#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ChiaKey::WindowsTsf {

class TextService;

// GUID_LBI_INPUTMODE, the system's tray input indicator
inline constexpr GUID kLangBarInputModeGuid = {
    0x2c77a81e, 0x41cc, 0x4178, {0xa3, 0xa7, 0x5f, 0x8a, 0x98, 0x75, 0x68, 0xe6}};
// {93CF9168-BD86-455D-91A5-1A35CA1E9B8D}
inline constexpr GUID kLangBarSwitchLanguageGuid = {
    0x93cf9168, 0xbd86, 0x455d, {0x91, 0xa5, 0x1a, 0x35, 0xca, 0x1e, 0x9b, 0x8d}};
// {07486078-6A01-47CD-BC66-EC5AD24891FD}
inline constexpr GUID kLangBarFullHalfGuid = {
    0x07486078, 0x6a01, 0x47cd, {0xbc, 0x66, 0xec, 0x5a, 0xd2, 0x48, 0x91, 0xfd}};

class LangBarButton final : public ITfLangBarItemButton, public ITfSource {
public:
    enum class Kind { InputMode, SwitchLanguage, FullHalf };

    LangBarButton(TextService* service, REFGUID guid, Kind kind);

    STDMETHODIMP QueryInterface(REFIID iid, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* info) override;
    STDMETHODIMP GetStatus(DWORD* status) override;
    STDMETHODIMP Show(BOOL show) override;
    STDMETHODIMP GetTooltipString(BSTR* tooltip) override;

    STDMETHODIMP OnClick(TfLBIClick click, POINT point, const RECT* area) override;
    STDMETHODIMP InitMenu(ITfMenu* menu) override;
    STDMETHODIMP OnMenuSelect(UINT id) override;
    STDMETHODIMP GetIcon(HICON* icon) override;
    STDMETHODIMP GetText(BSTR* text) override;

    STDMETHODIMP AdviseSink(REFIID iid, IUnknown* unknown, DWORD* cookie) override;
    STDMETHODIMP UnadviseSink(DWORD cookie) override;

    void update();

private:
    ~LangBarButton();
    const wchar_t* label() const;

    // one popup menu entry; id 0 draws a separator
    struct MenuItem {
        UINT id;
        std::wstring label;
        bool checked;
    };
    std::vector<MenuItem> menuItems();

    std::atomic<ULONG> referenceCount_{1};
    TextService* service_ = nullptr;
    GUID guid_{};
    Kind kind_;
    DWORD nextCookie_ = 1;
    std::mutex sinksMutex_;
    std::vector<std::pair<DWORD, ITfLangBarItemSink*>> sinks_;
    // menu ids past kMenuFirstInputMethod index into this
    std::vector<std::string> menuInputMethods_;
};

}  // namespace ChiaKey::WindowsTsf
