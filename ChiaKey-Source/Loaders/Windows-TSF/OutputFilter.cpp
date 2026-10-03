#include "OutputFilter.h"

#include <iterator>

// The same sorted character pairs used by Mac's OVOFHanConvert-TC2SC.
extern "C" unsigned short vxTC2SCTable[3059 * 2];

namespace ChiaKey::WindowsTsf {

std::wstring FilterCommittedText(std::wstring text, bool simplified) {
    if (!simplified) return text;
    for (wchar_t& character : text) {
        const auto key = static_cast<unsigned short>(character);
        // Preserve supplementary characters as their original UTF-16 pair.
        if (key >= 0xD800 && key <= 0xDFFF) continue;
        size_t first = 0, last = std::size(vxTC2SCTable) / 2;
        while (first < last) {
            const size_t middle = first + (last - first) / 2;
            if (vxTC2SCTable[middle * 2] < key) first = middle + 1;
            else last = middle;
        }
        if (first < std::size(vxTC2SCTable) / 2 && vxTC2SCTable[first * 2] == key)
            character = static_cast<wchar_t>(vxTC2SCTable[first * 2 + 1]);
    }
    return text;
}

}  // namespace ChiaKey::WindowsTsf
