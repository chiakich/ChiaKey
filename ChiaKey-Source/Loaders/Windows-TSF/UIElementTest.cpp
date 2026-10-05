#include "UIElement.h"
#include "ModuleState.h"
#include "EditSessionRequest.h"
#include <iostream>
#include <stdexcept>
#include <oleauto.h>

using namespace ChiaKey::WindowsTsf;
using Microsoft::WRL::ComPtr;
std::atomic<long> ChiaKey::WindowsTsf::g_objectCount{0};
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
class GameUIManager final : public ITfUIElementMgr {
public:
    ULONG refs = 1;
    bool allowWindow = false, rejectBegin = false;
    int begins = 0, updates = 0, ends = 0;
    ComPtr<ITfUIElement> retained;
    STDMETHODIMP QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_INVALIDARG;
        *object = nullptr;
        if (iid != IID_IUnknown && iid != IID_ITfUIElementMgr) return E_NOINTERFACE;
        *object = static_cast<ITfUIElementMgr*>(this); AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++refs; }
    STDMETHODIMP_(ULONG) Release() override { ULONG n = --refs; if (!n) delete this; return n; }
    STDMETHODIMP BeginUIElement(ITfUIElement* element, BOOL* show, DWORD* id) override {
        ++begins;
        if (rejectBegin) return E_FAIL;
        retained = element; *show = allowWindow; *id = 17;
        ComPtr<ITfCandidateListUIElement> list;
        if (SUCCEEDED(retained.As(&list))) {
            UINT count = 0;
            Check(SUCCEEDED(list->GetCount(&count)) && count == 5, "data unavailable during BeginUIElement");
        }
        return S_OK;
    }
    STDMETHODIMP UpdateUIElement(DWORD id) override { Check(id == 17, "wrong update ID"); ++updates; return S_OK; }
    STDMETHODIMP EndUIElement(DWORD id) override { Check(id == 17, "wrong end ID"); ++ends; return S_OK; }
    STDMETHODIMP GetUIElement(DWORD, ITfUIElement** element) override { return retained.CopyTo(element); }
    STDMETHODIMP EnumUIElements(IEnumTfUIElements**) override { return E_NOTIMPL; }
};
void Run() {
    for (const HRESULT refusal : {TF_E_SYNCHRONOUS, TF_E_LOCKED}) {
        for (const bool inner : {false, true}) {
            int calls = 0; HRESULT edit = E_FAIL; bool retried = false;
            const HRESULT status = RequestWriteEditSession([&](DWORD flags, HRESULT* result) -> HRESULT {
                ++calls;
                Check(flags == static_cast<DWORD>((calls == 1 ? TF_ES_SYNC : TF_ES_ASYNCDONTCARE) | TF_ES_READWRITE), "wrong edit session flags");
                if (calls == 1) { *result = inner ? refusal : E_FAIL; return inner ? S_OK : refusal; }
                *result = TF_S_ASYNC; return S_OK;
            }, &edit, &retried);
            Check(status == S_OK && edit == TF_S_ASYNC && retried && calls == 2, "transient session refusal not retried");
        }
    }
    for (const HRESULT response : {S_OK, TF_E_READONLY, E_FAIL}) {
        int calls = 0; HRESULT edit = E_FAIL; bool retried = true;
        RequestWriteEditSession([&](DWORD, HRESULT* result) -> HRESULT { ++calls; *result = response; return S_OK; }, &edit, &retried);
        Check(calls == 1 && !retried && edit == response, "nontransient session retried");
    }
    const GUID guid = {0x93767939, 0x30ea, 0x4b4f, {0x9a, 0x83, 1, 2, 3, 4, 5, 6}};
    ComPtr<GameUIManager> manager; manager.Attach(new GameUIManager);
    ComPtr<TextUIElement> element; element.Attach(new TextUIElement(true, guid, L"Candidates"));
    // No document geometry or actual windows: this models a game-rendered list.
    element->update(nullptr, {L"one", L"two", L"three", L"four", L"five"}, 3, 2);
    bool visible = false;
    element->setCallbacks([&](bool show) { visible = show; }, [&] { return visible; });
    UIElementSession session;
    Check(SUCCEEDED(session.present(manager.Get(), element.Get(), false)), "begin failed");
    Check(!visible && manager->begins == 1 && manager->updates == 1, "host rendering must suppress local window and receive initial update");
    UINT page = 0, selection = 0, count = 0;
    Check(SUCCEEDED(element->GetSelection(&selection)) && selection == 3, "wrong global selection");
    Check(SUCCEEDED(element->GetCurrentPage(&page)) && page == 1, "wrong current page");
    Check(element->GetPageIndex(nullptr, 0, &count) == S_FALSE && count == 3, "page count query failed");
    UINT indices[3]{};
    Check(SUCCEEDED(element->GetPageIndex(indices, 3, &count)) && indices[2] == 4, "wrong page boundaries");
    BSTR string = nullptr;
    Check(SUCCEEDED(element->GetString(4, &string)) && std::wstring(string) == L"five", "host cannot read candidates outside local page");
    SysFreeString(string);
    Check(element->GetString(5, &string) == E_INVALIDARG && !string, "invalid candidate index accepted");
    UINT invalid[] = {0, 3, 2};
    Check(element->SetPageIndex(invalid, 3) == E_INVALIDARG, "invalid pagination accepted");
    UINT custom[] = {0, 3};
    Check(SUCCEEDED(element->SetPageIndex(custom, 2)), "host pagination rejected");
    Check(SUCCEEDED(element->GetCurrentPage(&page)) && page == 1, "custom page selection incorrect");
    element->update(nullptr, {L"one", L"two", L"three", L"four", L"five"}, 4, 2);
    Check(SUCCEEDED(element->GetCurrentPage(&page)) && page == 1, "host pagination lost on selection update");
    Check(element->GetCount(nullptr) == E_INVALIDARG && element->IsShown(nullptr) == E_INVALIDARG, "null output accepted");
    element->Show(TRUE);
    Check(visible, "host cannot restore local UI");
    element->Show(FALSE);
    Check(SUCCEEDED(session.present(manager.Get(), element.Get(), true)) && !visible && manager->begins == 1 && manager->updates == 2,
          "host Show(FALSE) lost during update");
    element->update(nullptr, {L"a", L"b", L"c", L"d", L"e"}, 4, 2);
    Check(SUCCEEDED(session.present(manager.Get(), element.Get(), false)), "candidate update failed");
    Check(SUCCEEDED(element->GetCurrentPage(&page)) && page == 2, "engine pagination update lost");
    session.end();
    Check(manager->ends == 1 && !visible, "EndUIElement missing");
    // Hosts may retain a COM object after EndUIElement or TIP destruction.
    manager->retained->Show(TRUE);
    BOOL shown = TRUE;
    manager->retained->IsShown(&shown);
    Check(!visible && !shown, "detached object accessed TIP UI");
    session.end(); Check(manager->ends == 1, "duplicate EndUIElement");

    element->setCallbacks([&](bool show) { visible = show; }, [&] { return visible; });
    manager->allowWindow = true;
    Check(SUCCEEDED(session.present(manager.Get(), element.Get(), false)) && visible, "pbShow TRUE ignored");
    session.end();
    element->setCallbacks([&](bool show) { visible = show; }, [&] { return visible; });
    manager->rejectBegin = true;
    Check(FAILED(session.present(manager.Get(), element.Get(), true)) && !visible, "failed negotiation showed UI");
    session.end(); Check(manager->ends == 2, "ending failed BeginUIElement");

    ComPtr<TextUIElement> auxiliary; auxiliary.Attach(new TextUIElement(false, guid, L"Message"));
    ComPtr<ITfCandidateListUIElement> list;
    Check(auxiliary.As(&list) == E_NOINTERFACE, "auxiliary UI advertised candidate data");
    auxiliary->setCallbacks([&](bool show) { visible = show; }, [&] { return visible; });
    Check(SUCCEEDED(session.present(nullptr, auxiliary.Get(), true)) && visible, "desktop fallback failed");
    session.end();
    auxiliary->setCallbacks([&](bool show) { visible = show; }, [&] { return visible; });
    Check(SUCCEEDED(session.present(nullptr, auxiliary.Get(), false)) && !visible, "UILess fallback showed window");
    session.end();
}
}
int main() {
    try { Run(); Check(g_objectCount == 0, "UIElement references leaked"); std::cout << "UILess host negotiation and candidate lifecycle passed\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
