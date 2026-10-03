#pragma once
#include <string>
#include <vector>

namespace ChiaKey::WindowsTsf {
// Read the atomically replaced pointer. Candidates are current then previous;
// callers must fall back to the bundled DB if either cannot be opened.
std::vector<std::string> UpdateLexiconCandidates(const std::wstring& roaming);
}
