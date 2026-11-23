#include "OutputFilter.h"
#include "ChiaKeyEngine.h"
#include "TaiwanToSimplified.h"

#include <cstdint>

namespace ChiaKey::WindowsTsf {
namespace {

// UTF-16 to UTF-8 that keeps lone surrogates as three-byte sequences (WTF-8),
// so text the converter does not touch round-trips exactly.
std::string ToWtf8(const std::wstring& text) {
    std::string output;
    output.reserve(text.size() * 3);
    for (size_t index = 0; index < text.size(); ++index) {
        uint32_t code = static_cast<uint16_t>(text[index]);
        if (code >= 0xD800 && code <= 0xDBFF && index + 1 < text.size()) {
            const uint32_t low = static_cast<uint16_t>(text[index + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                ++index;
            }
        }
        if (code < 0x80) {
            output += static_cast<char>(code);
        } else if (code < 0x800) {
            output += static_cast<char>(0xC0 | (code >> 6));
            output += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            output += static_cast<char>(0xE0 | (code >> 12));
            output += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            output += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            output += static_cast<char>(0xF0 | (code >> 18));
            output += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            output += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            output += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
    return output;
}

std::wstring FromWtf8(const std::string& text) {
    std::wstring output;
    output.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index]);
        size_t length = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
        if (index + length > text.size()) length = 1;
        uint32_t code = length == 1 ? lead
                        : length == 2 ? lead & 0x1F
                        : length == 3 ? lead & 0x0F
                                      : lead & 0x07;
        for (size_t offset = 1; offset < length; ++offset) {
            code = (code << 6) | (static_cast<unsigned char>(text[index + offset]) & 0x3F);
        }
        if (code >= 0x10000) {
            code -= 0x10000;
            output += static_cast<wchar_t>(0xD800 + (code >> 10));
            output += static_cast<wchar_t>(0xDC00 + (code & 0x3FF));
        } else {
            output += static_cast<wchar_t>(code);
        }
        index += length;
    }
    return output;
}

}  // namespace

std::wstring ToFullWidth(std::wstring text) {
    for (wchar_t& character : text) {
        if (character == L' ') {
            character = L'　';
        } else if (character >= L'!' && character <= L'~') {
            character = static_cast<wchar_t>(character - L'!' + L'！');
        }
    }
    return text;
}

bool ApplyFullWidthFallback(wchar_t character, EngineResult& result) {
    if (result.handled || !result.compositionText.empty() || !result.committedText.empty() ||
        character < L' ' || character > L'~') return false;
    result = {};
    result.handled = true;
    result.committedText = ToFullWidth(std::wstring(1, character));
    return true;
}

std::wstring FilterCommittedText(std::wstring text, bool simplified) {
    if (!simplified || text.empty()) return text;
    return FromWtf8(ChiaKey::ConvertTaiwanToSimplified(ToWtf8(text)));
}

}  // namespace ChiaKey::WindowsTsf
