#pragma once

#include <string>

namespace ChiaKey::WindowsTsf {

// Like the Mac output filter, conversion affects committed text only.
std::wstring FilterCommittedText(std::wstring text, bool simplified);

}  // namespace ChiaKey::WindowsTsf
