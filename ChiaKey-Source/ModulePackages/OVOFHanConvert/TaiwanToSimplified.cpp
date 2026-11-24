#include "TaiwanToSimplified.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

struct TaiwanToSimplifiedEntry {
  uint16_t key;
  uint16_t value;
  uint8_t keyLength;
  uint8_t valueLength;
};

struct TaiwanToSimplifiedTable {
  const char* pool;
  const TaiwanToSimplifiedEntry* entries;
  size_t count;
  size_t maxKeyCharacters;
};

#include "TaiwanToSimplifiedTable.inc"

// Malformed bytes become one-byte characters that no key can match.
size_t CharacterLength(std::string_view text, size_t offset) {
  const unsigned char lead = static_cast<unsigned char>(text[offset]);
  size_t length = 1;
  if (lead >= 0xC2 && lead <= 0xDF) length = 2;
  else if (lead >= 0xE0 && lead <= 0xEF) length = 3;
  else if (lead >= 0xF0 && lead <= 0xF4) length = 4;
  if (offset + length > text.size()) return 1;
  for (size_t index = 1; index < length; ++index) {
    if ((static_cast<unsigned char>(text[offset + index]) & 0xC0) != 0x80) return 1;
  }
  return length;
}

struct Text {
  std::string bytes;
  // Byte offset of every character, plus the end.
  std::vector<size_t> starts;
  // Indexed by character position; true where an input segment begins or ends.
  std::vector<bool> boundaries;

  size_t size() const { return starts.size() - 1; }
  std::string_view slice(size_t begin, size_t end) const {
    return std::string_view(bytes).substr(starts[begin], starts[end] - starts[begin]);
  }
};

std::string_view Key(const TaiwanToSimplifiedTable& table,
                     const TaiwanToSimplifiedEntry& entry) {
  return std::string_view(table.pool + entry.key, entry.keyLength);
}

const TaiwanToSimplifiedEntry* Find(const TaiwanToSimplifiedTable& table,
                                    std::string_view key) {
  const TaiwanToSimplifiedEntry* end = table.entries + table.count;
  const TaiwanToSimplifiedEntry* found = std::lower_bound(
      table.entries, end, key,
      [&table](const TaiwanToSimplifiedEntry& entry, std::string_view wanted) {
        return Key(table, entry) < wanted;
      });
  return found != end && Key(table, *found) == key ? found : nullptr;
}

bool Permitted(const Text& text, size_t begin, size_t length) {
  if (text.boundaries[begin] && text.boundaries[begin + length]) return true;
  for (size_t index = begin + 1; index < begin + length; ++index) {
    if (text.boundaries[index]) return false;
  }
  return true;
}

// Longest key starting at begin that ends by limit, like OpenCC's MatchPrefix.
const TaiwanToSimplifiedEntry* Match(const TaiwanToSimplifiedTable& table,
                                     const Text& text, size_t begin, size_t limit,
                                     size_t* length) {
  for (size_t n = std::min(table.maxKeyCharacters, limit - begin); n > 0; --n) {
    if (!Permitted(text, begin, n)) continue;
    if (const TaiwanToSimplifiedEntry* entry = Find(table, text.slice(begin, begin + n))) {
      *length = n;
      return entry;
    }
  }
  return nullptr;
}

typedef std::vector<std::pair<size_t, size_t>> Spans;

// OpenCC's mmseg: TSPhrases words become their own spans, the rest run together.
Spans Segment(const Text& text) {
  Spans spans;
  size_t start = 0;
  for (size_t index = 0; index < text.size();) {
    size_t length = 0;
    if (Match(kSegmentation, text, index, text.size(), &length)) {
      if (start < index) spans.emplace_back(start, index);
      spans.emplace_back(index, index + length);
      index += length;
      start = index;
    } else {
      ++index;
    }
  }
  if (start < text.size()) spans.emplace_back(start, text.size());
  return spans;
}

// Every entry keeps its character count, so boundaries carry over unchanged.
Text Convert(const TaiwanToSimplifiedTable& table, const Text& text, const Spans& spans) {
  Text output;
  output.boundaries = text.boundaries;
  output.bytes.reserve(text.bytes.size());
  for (const auto& span : spans) {
    for (size_t index = span.first; index < span.second;) {
      size_t length = 0;
      const TaiwanToSimplifiedEntry* entry = Match(table, text, index, span.second, &length);
      if (!entry) {
        output.starts.push_back(output.bytes.size());
        output.bytes.append(text.slice(index, index + 1));
        ++index;
        continue;
      }
      const std::string_view value(table.pool + entry->value, entry->valueLength);
      for (size_t offset = 0; offset < value.size(); offset += CharacterLength(value, offset)) {
        output.starts.push_back(output.bytes.size() + offset);
      }
      output.bytes.append(value);
      index += length;
    }
  }
  output.starts.push_back(output.bytes.size());
  return output;
}

}  // namespace

namespace ChiaKey {

std::vector<std::string> ConvertTaiwanToSimplified(
    const std::vector<std::string>& segments) {
  Text text;
  std::vector<size_t> segmentEnds;
  text.boundaries.push_back(true);
  for (const std::string& segment : segments) {
    for (size_t offset = 0; offset < segment.size();
         offset += CharacterLength(segment, offset)) {
      text.starts.push_back(text.bytes.size() + offset);
      text.boundaries.push_back(false);
    }
    text.bytes += segment;
    text.boundaries.back() = true;
    segmentEnds.push_back(text.starts.size());
  }
  text.starts.push_back(text.bytes.size());

  if (text.size() > 0) {
    // OpenCC normalizes the whole text before it segments.
    text = Convert(kNormalization, text, Spans{{0, text.size()}});
    const Spans spans = Segment(text);
    text = Convert(kVariants, text, spans);
    text = Convert(kSimplify, text, spans);
  }

  std::vector<std::string> output;
  size_t begin = 0;
  for (size_t end : segmentEnds) {
    output.emplace_back(text.slice(begin, end));
    begin = end;
  }
  return output;
}

std::string ConvertTaiwanToSimplified(std::string_view text) {
  if (text.empty()) return std::string();
  return ConvertTaiwanToSimplified(std::vector<std::string>{std::string(text)}).front();
}

}  // namespace ChiaKey
