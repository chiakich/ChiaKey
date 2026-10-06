#pragma once

#include <Windows.h>
#include <msctf.h>
#include <string>

namespace ChiaKey::WindowsTsf {

using TextServiceFactory = HRESULT (*)(ITfTextInputProcessorEx**, std::wstring* revision);
using TextServiceRevision = std::wstring (*)();

// The outer COM identity survives a TSF deactivate/activate cycle. Factories
// return a reference owned by the caller; failure must leave the output null.
HRESULT CreateReloadableTextService(IUnknown* outer, REFIID iid, void** object,
                                   TextServiceFactory latest, TextServiceFactory fallback,
                                   TextServiceRevision publishedRevision);

} // namespace ChiaKey::WindowsTsf
