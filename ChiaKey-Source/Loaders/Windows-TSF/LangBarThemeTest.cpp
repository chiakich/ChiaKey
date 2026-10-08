#include "LangBarButton.h"
#include "ModuleState.h"
#include "TextService.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ChiaKey::WindowsTsf {
HMODULE g_module = nullptr;
std::atomic<long> g_objectCount{0}, g_serverLocks{0};
}
using namespace ChiaKey::WindowsTsf;
using Microsoft::WRL::ComPtr;
void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
    std::cout << "PASS: " << message << '\n';
}

// Redirect HKCU only in this test process; never change the user's theme.
class ThemeRegistry {
    HKEY root_ = nullptr;
    std::wstring path_ = L"Software\\ChiaKey\\ThemeTest-" + std::to_wstring(GetCurrentProcessId());
public:
    ThemeRegistry() {
        Check(RegCreateKeyExW(HKEY_CURRENT_USER, path_.c_str(), 0, nullptr, 0,
                             KEY_ALL_ACCESS, nullptr, &root_, nullptr) == ERROR_SUCCESS,
              "create isolated theme registry");
        Check(RegOverridePredefKey(HKEY_CURRENT_USER, root_) == ERROR_SUCCESS, "redirect test HKCU");
    }
    ~ThemeRegistry() {
        RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        RegCloseKey(root_);
        RegDeleteTreeW(HKEY_CURRENT_USER, path_.c_str());
    }
    void set(DWORD light) {
        HKEY key = nullptr;
        Check(RegCreateKeyExW(root_, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
              0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS, "open test theme");
        const auto result = RegSetValueExW(key, L"SystemUsesLightTheme", 0, REG_DWORD,
                                         reinterpret_cast<const BYTE*>(&light), sizeof(light));
        RegCloseKey(key);
        Check(result == ERROR_SUCCESS, "set isolated system theme");
    }
};

class Sink final : public ITfLangBarItemSink {
    ULONG refs_ = 1;
public:
    unsigned updates = 0;
    STDMETHODIMP QueryInterface(REFIID iid, void** out) override {
        *out = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfLangBarItemSink) return E_NOINTERFACE;
        *out = static_cast<ITfLangBarItemSink*>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs_; }
    STDMETHODIMP_(ULONG) Release() override { auto n = --refs_; if (!n) delete this; return n; }
    STDMETHODIMP OnUpdate(DWORD flags) override { if (flags & TF_LBI_ICON) ++updates; return S_OK; }
};

void CheckIcon(LangBarButton* button, bool light) {
    HICON icon = nullptr;
    Check(SUCCEEDED(button->GetIcon(&icon)) && icon, "load production mode icon");
    ICONINFO info{};
    const BOOL gotInfo = GetIconInfo(icon, &info);
    DestroyIcon(icon);
    Check(gotInfo && info.hbmColor, "mode icon has alpha color bitmap");
    BITMAP bitmap{};
    GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap);
    BITMAPINFO dib{};
    dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dib.bmiHeader.biWidth = bitmap.bmWidth;
    dib.bmiHeader.biHeight = -bitmap.bmHeight;
    dib.bmiHeader.biPlanes = 1;
    dib.bmiHeader.biBitCount = 32;
    std::vector<DWORD> pixels(bitmap.bmWidth * bitmap.bmHeight);
    HDC dc = CreateCompatibleDC(nullptr);
    const int rows = GetDIBits(dc, info.hbmColor, 0, bitmap.bmHeight, pixels.data(), &dib, DIB_RGB_COLORS);
    DeleteDC(dc); DeleteObject(info.hbmColor); DeleteObject(info.hbmMask);
    Check(rows == bitmap.bmHeight, "read icon pixels");
    unsigned glyph = 0, transparent = 0, wrong = 0;
    for (DWORD pixel : pixels) {
        if ((pixel >> 24) == 0) ++transparent;
        if ((pixel >> 24) > 240) {
            ++glyph;
            const DWORD rgb = pixel & 0xffffff;
            if (light ? rgb != 0 : (rgb & 0xff) < 240 || ((rgb >> 8) & 0xff) < 240 || (rgb >> 16) < 240)
                ++wrong;
        }
    }
    Check(glyph && transparent && !wrong, light ? "transparent black glyph on light taskbar"
                                              : "transparent white glyph on dark taskbar");
}

void Pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 0;
    try {
        ThemeRegistry theme;
        theme.set(1);
        g_module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_AS_DATAFILE);
        Check(g_module != nullptr, "load actual DLL resources");
        ComPtr<TextService> service; service.Attach(new TextService);
        ComPtr<LangBarButton> button;
        button.Attach(new LangBarButton(service.Get(), kLangBarInputModeGuid, LangBarButton::Kind::InputMode));
        ComPtr<Sink> sink; sink.Attach(new Sink);
        DWORD cookie = 0;
        Check(SUCCEEDED(button->AdviseSink(IID_ITfLangBarItemSink, sink.Get(), &cookie)), "attach TSF sink");
        Check(SUCCEEDED(button->startThemeTracking()), "start hidden theme listener");
        // The resource-only module handle is valid for icons and the system STATIC class.
        HWND window = nullptr;
        EnumThreadWindows(GetCurrentThreadId(), [](HWND candidate, LPARAM out) -> BOOL {
            wchar_t title[64]{};
            GetWindowTextW(candidate, title, 64);
            if (wcscmp(title, L"ChiaKey input mode theme") != 0) return TRUE;
            *reinterpret_cast<HWND*>(out) = candidate;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&window));
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        Check(window && process == GetCurrentProcessId(), "listener belongs to test process");
        TF_LANGBARITEMINFO info{};
        button->GetInfo(&info);
        Check(!(info.dwStyle & TF_LBI_STYLE_TEXTCOLORICON), "alpha icons do not request monochrome recoloring");
        CheckIcon(button.Get(), true);
        for (unsigned cycle = 0; cycle < 4; ++cycle) {
            const bool light = (cycle % 2) != 0;
            theme.set(light);
            SendMessageW(window, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
            Check(sink->updates == cycle, "broadcast defers TSF callback");
            Pump();
            Check(sink->updates == cycle + 1, "theme change notifies TSF exactly once");
            CheckIcon(button.Get(), light);
            SendMessageW(window, WM_THEMECHANGED, 0, 0); Pump();
            Check(sink->updates == cycle + 1, "unchanged theme does not repeat notifications");
        }
        theme.set(0);
        SendMessageW(window, WM_SETTINGCHANGE, 0, 0);
        button->stopThemeTracking(); Pump();
        Check(!IsWindow(window) && sink->updates == 4, "deactivation cancels queued refresh and destroys listener");
        button->UnadviseSink(cookie);
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; result = 1; }
    if (g_module) FreeLibrary(g_module);
    CoUninitialize();
    return result;
}
