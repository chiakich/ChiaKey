#pragma once

#include <string>

namespace ChiaKey::WindowsTsf {

// Like the Mac output filter, conversion affects committed text only.
std::wstring FilterCommittedText(std::wstring text, bool simplified);

std::wstring ToFullWidth(std::wstring text);
struct EngineResult;
// Only provide direct text when an unhandled key left no live composition.
bool ApplyFullWidthFallback(wchar_t character, EngineResult& result);

}  // namespace ChiaKey::WindowsTsf
