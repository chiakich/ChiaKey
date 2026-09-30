#include <Windows.h>
#include <msctf.h>

#include <iterator>
#include <new>
#include <string>

#include "Guids.h"
#include "ModuleState.h"
#include "TextService.h"

using namespace ChiaKey::WindowsTsf;

HMODULE ChiaKey::WindowsTsf::g_module = nullptr;
std::atomic<long> ChiaKey::WindowsTsf::g_objectCount{0};
std::atomic<long> ChiaKey::WindowsTsf::g_serverLocks{0};

namespace {

class ClassFactory final : public IClassFactory {
public:
    ClassFactory() { ++g_objectCount; }

    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_IClassFactory) {
            *object = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID iid, void** object) override {
        return TextService::CreateInstance(outer, iid, object);
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        lock ? ++g_serverLocks : --g_serverLocks;
        return S_OK;
    }

private:
    ~ClassFactory() { --g_objectCount; }
    std::atomic<ULONG> references_{1};
};

std::wstring GuidString(REFGUID guid) {
    wchar_t text[40]{};
    return StringFromGUID2(guid, text, static_cast<int>(std::size(text))) ? text : L"";
}

HRESULT SetRegistryString(HKEY root, const std::wstring& keyPath,
                          const wchar_t* valueName, const std::wstring& value) {
    HKEY key = nullptr;
    const LONG opened = RegCreateKeyExW(root, keyPath.c_str(), 0, nullptr, 0, KEY_WRITE,
                                        nullptr, &key, nullptr);
    if (opened != ERROR_SUCCESS) return HRESULT_FROM_WIN32(opened);
    const LONG written = RegSetValueExW(
        key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(written);
}

std::wstring ModulePath() {
    wchar_t path[32768]{};
    const DWORD length =
        GetModuleFileNameW(g_module, path, static_cast<DWORD>(std::size(path)));
    return length && length < std::size(path) ? std::wstring(path, length) : std::wstring();
}

HRESULT RegisterComServer() {
    const std::wstring modulePath = ModulePath();
    if (modulePath.empty()) return HRESULT_FROM_WIN32(GetLastError());

    const std::wstring classKey = L"CLSID\\" + GuidString(kTextServiceClsid);
    HRESULT result =
        SetRegistryString(HKEY_CLASSES_ROOT, classKey, nullptr, kTextServiceDescription);
    if (FAILED(result)) return result;
    result = SetRegistryString(HKEY_CLASSES_ROOT, classKey + L"\\InprocServer32", nullptr,
                               modulePath);
    if (FAILED(result)) return result;
    return SetRegistryString(HKEY_CLASSES_ROOT, classKey + L"\\InprocServer32",
                             L"ThreadingModel", kThreadingModel);
}

void UnregisterComServer() {
    const std::wstring classKey = L"CLSID\\" + GuidString(kTextServiceClsid);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, classKey.c_str());
}

HRESULT RegisterProfile() {
    const std::wstring modulePath = ModulePath();
    if (modulePath.empty()) return HRESULT_FROM_WIN32(GetLastError());

    ITfInputProcessorProfileMgr* profiles = nullptr;
    HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles));
    if (FAILED(result)) return result;
    result = profiles->RegisterProfile(
        kTextServiceClsid, kTraditionalChineseLangId, kTraditionalChineseProfileGuid,
        kTextServiceDescription, static_cast<ULONG>(wcslen(kTextServiceDescription)),
        modulePath.c_str(), static_cast<ULONG>(modulePath.size()), 0, nullptr, 0, TRUE, 0);
    profiles->Release();
    return result;
}

void UnregisterProfile() {
    ITfInputProcessorProfileMgr* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&profiles)))) {
        profiles->UnregisterProfile(kTextServiceClsid, kTraditionalChineseLangId,
                                    kTraditionalChineseProfileGuid, 0);
        profiles->Release();
    }
}

// immersive support is what lets Start/Search and Store apps load the TIP
const GUID kCategories[] = {
    GUID_TFCAT_TIP_KEYBOARD,
    GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
};

HRESULT RegisterCategories() {
    ITfCategoryMgr* manager = nullptr;
    HRESULT result = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&manager));
    if (FAILED(result)) return result;
    for (const GUID& category : kCategories) {
        result = manager->RegisterCategory(kTextServiceClsid, category, kTextServiceClsid);
        if (FAILED(result)) break;
    }
    manager->Release();
    return result;
}

void UnregisterCategories() {
    ITfCategoryMgr* manager = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&manager)))) {
        for (const GUID& category : kCategories) {
            manager->UnregisterCategory(kTextServiceClsid, category, kTextServiceClsid);
        }
        manager->Release();
    }
}

class ComScope final {
public:
    ComScope() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope() {
        if (SUCCEEDED(result_)) CoUninitialize();
    }
    HRESULT result() const { return result_ == RPC_E_CHANGED_MODE ? S_OK : result_; }

private:
    HRESULT result_;
};

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return g_objectCount.load() == 0 && g_serverLocks.load() == 0 ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
    if (!object) return E_INVALIDARG;
    *object = nullptr;
    if (clsid != kTextServiceClsid) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT result = factory->QueryInterface(iid, object);
    factory->Release();
    return result;
}

extern "C" HRESULT __stdcall DllRegisterServer() {
    ComScope com;
    HRESULT result = com.result();
    if (FAILED(result)) return result;
    result = RegisterComServer();
    if (FAILED(result)) return result;
    result = RegisterProfile();
    if (FAILED(result)) {
        UnregisterComServer();
        return result;
    }
    result = RegisterCategories();
    if (FAILED(result)) {
        UnregisterProfile();
        UnregisterComServer();
    }
    return result;
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    ComScope com;
    if (FAILED(com.result())) return com.result();
    UnregisterCategories();
    UnregisterProfile();
    UnregisterComServer();
    return S_OK;
}
