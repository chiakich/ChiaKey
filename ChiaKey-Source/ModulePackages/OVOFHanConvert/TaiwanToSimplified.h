#ifndef TaiwanToSimplified_h
#define TaiwanToSimplified_h

#include <string>
#include <string_view>
#include <vector>

namespace ChiaKey {

// OpenCC's tw2s conversion plus TaiwanToSimplifiedOverrides.txt, on UTF-8.
std::string ConvertTaiwanToSimplified(std::string_view text);

// Segments are the input method's words; a dictionary phrase may cover whole
// words but never cut through one, so 生活|著|名為 never matches 著名.
// Returns one converted string per segment.
std::vector<std::string> ConvertTaiwanToSimplified(
    const std::vector<std::string>& segments);

}  // namespace ChiaKey

#endif
