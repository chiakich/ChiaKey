#pragma once
#include <Windows.h>
#include <msctf.h>

namespace ChiaKey::WindowsTsf {
// TSF reports refusal in either the call HRESULT or the session HRESULT.
// Only lock/synchronous refusal is transient; read-only and other errors pass through.
template<class Request>
HRESULT RequestWriteEditSession(Request request, HRESULT* editResult, bool* retriedAsync = nullptr) {
    *editResult = E_FAIL;
    HRESULT result = request(TF_ES_SYNC | TF_ES_READWRITE, editResult);
    const bool refused = result == TF_E_SYNCHRONOUS || result == TF_E_LOCKED ||
        (SUCCEEDED(result) && (*editResult == TF_E_SYNCHRONOUS || *editResult == TF_E_LOCKED));
    if (refused) {
        *editResult = E_FAIL;
        result = request(TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, editResult);
    }
    if (retriedAsync) *retriedAsync = refused;
    return result;
}
} // namespace ChiaKey::WindowsTsf
