#include <Windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <algorithm>
#include "Guids.h"
#include "ReloadControl.h"

using Microsoft::WRL::ComPtr;
using namespace ChiaKey::WindowsTsf;

void Check(bool pass, const char* message) {
    if (!pass) throw std::runtime_error(message);
    std::cout << "PASS: " << message << std::endl;
}
void Ok(HRESULT hr, const char* message) { Check(SUCCEEDED(hr), message); }

void PumpFor(DWORD milliseconds) {
    const ULONGLONG end = GetTickCount64() + milliseconds;
    do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(10);
    } while (GetTickCount64() < end);
}

// Override HKLM only in this test process. Never alter the installed IME.
class RegistrySandbox {
public:
    RegistrySandbox() {
        root_ = L"Software\\ChiaKeyReloadTest-" + std::to_wstring(GetCurrentProcessId());
        Check(RegCreateKeyExW(HKEY_CURRENT_USER, root_.c_str(), 0, nullptr, 0,
                             KEY_ALL_ACCESS, nullptr, &key_, nullptr) == ERROR_SUCCESS, "create sandbox");
        Check(RegOverridePredefKey(HKEY_LOCAL_MACHINE, key_) == ERROR_SUCCESS, "override HKLM");
    }
    ~RegistrySandbox() {
        RegOverridePredefKey(HKEY_LOCAL_MACHINE, nullptr);
        RegCloseKey(key_);
        RegDeleteTreeW(HKEY_CURRENT_USER, root_.c_str());
    }
    void publish(const wchar_t* dll) {
        std::wstring nativePath(dll);
        std::replace(nativePath.begin(), nativePath.end(), L'/', L'\\');
        const std::wstring path = L"Software\\ChiaKey\\Tsf";
        HKEY value = nullptr;
        Check(RegCreateKeyExW(key_, path.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr,
                             &value, nullptr) == ERROR_SUCCESS, "create registration");
        const auto result = RegSetValueExW(value, L"BackendPathV1", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(nativePath.c_str()),
            static_cast<DWORD>((nativePath.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(value);
        Check(result == ERROR_SUCCESS, "publish registration");
    }
private:
    HKEY key_ = nullptr;
    std::wstring root_;
};

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        ComPtr<ITfThreadMgr> threadManager;
        Ok(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
                            IID_PPV_ARGS(&threadManager)), "create inactive thread manager");
        RegistrySandbox registry;
        const HMODULE loader = LoadLibraryW(argv[1]);
        if (!loader) throw std::runtime_error("load production DLL, Win32 error " +
                                              std::to_string(GetLastError()));
        Check(true, "load production DLL");
        using GetClass = HRESULT (WINAPI*)(REFCLSID, REFIID, void**);
        const auto getClass = reinterpret_cast<GetClass>(GetProcAddress(loader, "DllGetClassObject"));
        Check(getClass != nullptr, "COM factory export");
        Check(GetProcAddress(loader, "ChiaKeyCreateTextServiceV1") != nullptr, "backend ABI export");
        ComPtr<IClassFactory> factory;
        Ok(getClass(kTextServiceClsid, IID_PPV_ARGS(&factory)), "get class");
        {
            ComPtr<ITfFunctionProvider> local;
            Ok(factory->CreateInstance(nullptr, IID_PPV_ARGS(&local)), "initial local fallback without publication");
            GUID value{}; Ok(local->GetType(&value), "local type");
            Check(value == kTextServiceClsid, "fallback is production backend");
        }
        registry.publish(argv[2]);
        ComPtr<ITfTextInputProcessorEx> tip;
        Ok(factory->CreateInstance(nullptr, IID_PPV_ARGS(&tip)), "create proxy");
        ComPtr<ITfFunctionProvider> provider;
        Ok(tip.As(&provider), "cached function provider");
        ComPtr<IUnknown> identity, otherIdentity;
        Ok(tip.As(&identity), "TIP identity");
        Ok(provider.As(&otherIdentity), "provider identity");
        Check(identity == otherIdentity, "stable COM identity");
        auto version = [&] (DWORD expected, WORD flags) {
            GUID value{}; Ok(provider->GetType(&value), "backend marker");
            Check(value.Data1 == expected && value.Data2 == flags, "expected version/flags");
        };
        auto* manager = threadManager.Get(); // retained by loader; never activated by this test
        version(1, 0); // fail safely before passing the fixture-only manager
        Check(tip->ActivateEx(nullptr, 1, 0) == E_INVALIDARG, "reject null manager");
        Ok(tip->ActivateEx(manager, 1, TF_TMAE_UIELEMENTENABLEDONLY), "activate v1");
        version(1, TF_TMAE_UIELEMENTENABLEDONLY);
        registry.publish(argv[3]);
        Ok(tip->Activate(manager, 1), "duplicate activation");
        version(1, TF_TMAE_UIELEMENTENABLEDONLY); // never switch mid-composition
        Ok(tip->Deactivate(), "deactivate v1");
        Ok(tip->Deactivate(), "idempotent deactivate");
        Ok(tip->ActivateEx(manager, 1, TF_TMAE_SECUREMODE), "activate v2 without restarting");
        version(2, TF_TMAE_SECUREMODE); // same cached provider and TIP object
        Ok(tip->Deactivate(), "deactivate v2");
        registry.publish(L"C:\\missing-chiakey-reload-test\\ChiaKeyTsf.dll");
        Ok(tip->Activate(manager, 1), "missing update retains v2"); version(2, 0);
        Ok(tip->Deactivate(), "deactivate fallback");
        registry.publish(L"relative.dll");
        Ok(tip->Activate(manager, 1), "reject relative update and retain v2"); version(2, 0);
        Ok(tip->Deactivate(), "deactivate relative fallback");
        wchar_t system[MAX_PATH]{};
        Check(GetSystemDirectoryW(system, MAX_PATH) != 0, "system directory");
        registry.publish((std::wstring(system) + L"\\version.dll").c_str());
        Ok(tip->Activate(manager, 1), "missing ABI retains v2"); version(2, 0);
        Ok(tip->Deactivate(), "deactivate ABI fallback");
        const HMODULE first = LoadLibraryW(argv[2]);
        using Fail = void (WINAPI*)(BOOL);
        const auto fail = reinterpret_cast<Fail>(GetProcAddress(first, "FailActivation"));
        Check(fail != nullptr, "fixture failure control"); fail(TRUE);
        registry.publish(argv[2]);
        Ok(tip->Activate(manager, 1), "activation failure retains v2"); version(2, 0);
        Ok(tip->Deactivate(), "deactivate failure fallback");
        fail(FALSE);
        Ok(tip->Activate(manager, 1), "retry after failure / rollback"); version(1, 0);
        Ok(tip->Deactivate(), "final deactivate");
        const HMODULE second = LoadLibraryW(argv[3]);
        const auto busy = reinterpret_cast<Fail>(GetProcAddress(first, "SetBusy"));
        const auto failRestore = reinterpret_cast<Fail>(GetProcAddress(second, "FailRestore"));
        using SetState = void (WINAPI*)(DWORD);
        const auto setState = reinterpret_cast<SetState>(GetProcAddress(first, "SetInputState"));
        using Count = int (WINAPI*)();
        const auto count1 = reinterpret_cast<Count>(GetProcAddress(first, "ActiveCount"));
        const auto count2 = reinterpret_cast<Count>(GetProcAddress(second, "ActiveCount"));
        const auto attempts1 = reinterpret_cast<Count>(GetProcAddress(first, "ActivationAttempts"));
        const auto attempts2 = reinterpret_cast<Count>(GetProcAddress(second, "ActivationAttempts"));
        using DeactivateTarget = void (WINAPI*)(ITfTextInputProcessor*);
        const auto deactivateDuringActivation = reinterpret_cast<DeactivateTarget>(
            GetProcAddress(first, "DeactivateDuringActivation"));
        Check(busy && failRestore && setState && count1 && count2 && attempts1 && attempts2 &&
              deactivateDuringActivation, "automatic fixture controls");
        auto waitVersion = [&](DWORD expected) {
            const auto end = GetTickCount64() + 8000;
            GUID value{};
            do {
                PumpFor(50);
                if (FAILED(provider->GetType(&value))) throw std::runtime_error("automatic backend marker");
                if (value.Data1 == expected) break;
            } while (GetTickCount64() < end);
            Check(value.Data1 == expected, "automatically loaded published version");
            Check(value.Data2 == TF_TMAE_UIELEMENTENABLEDONLY && value.Data3 == kReloadFullWidth,
                  "automatic reload retains activation flags and English/full-width state");
            Check(count1() + count2() == 1, "exactly one active backend");
        };
        Ok(tip->ActivateEx(manager, 1, TF_TMAE_UIELEMENTENABLEDONLY), "start automatic monitoring");
        setState(kReloadFullWidth);
        busy(TRUE); registry.publish(argv[3]); PumpFor(1500);
        version(1, TF_TMAE_UIELEMENTENABLEDONLY);
        Check(count1() == 1 && count2() == 0, "busy backend is not deactivated");
        busy(FALSE); waitVersion(2); // no manual Deactivate or Activate
        const int failedAttempt = attempts1();
        fail(TRUE); registry.publish(argv[2]); PumpFor(1500);
        version(2, TF_TMAE_UIELEMENTENABLEDONLY);
        Check(attempts1() > failedAttempt, "automatic activation failure was exercised");
        Check(count1() == 0 && count2() == 1, "failed update restores previous active backend");
        fail(FALSE); waitVersion(1);
        const int restoreAttempt = attempts2();
        failRestore(TRUE); registry.publish(argv[3]); PumpFor(1500);
        version(1, TF_TMAE_UIELEMENTENABLEDONLY);
        Check(attempts2() > restoreAttempt, "automatic restore failure was exercised");
        Check(count1() == 1 && count2() == 0, "state restore failure rolls back");
        failRestore(FALSE); waitVersion(2);
        // TSF can deactivate the outer object during a nested activation call.
        // The new backend must not remain active after that request returns.
        deactivateDuringActivation(tip.Get()); registry.publish(argv[2]); PumpFor(1500);
        Check(count1() == 0 && count2() == 0, "reentrant TSF deactivation cancels monitoring");
        Ok(tip->ActivateEx(manager, 1, TF_TMAE_UIELEMENTENABLEDONLY), "reactivate after nested deactivation");
        Ok(tip->Deactivate(), "stop automatic monitoring");
        registry.publish(argv[3]); PumpFor(1500);
        version(1, TF_TMAE_UIELEMENTENABLEDONLY);
        Check(count1() == 0 && count2() == 0, "inactive TIP does not reactivate on timer");
        FreeLibrary(first); FreeLibrary(second);
        std::cout << "Manual and automatic same-process reload, deferral, state and rollback passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; CoUninitialize(); return 1;
    }
    CoUninitialize();
    return 0;
}
