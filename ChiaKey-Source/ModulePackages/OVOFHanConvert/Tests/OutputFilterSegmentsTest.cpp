#include <iostream>
#include <memory>

#include "OVOFFullWidthCharacter.h"
#include "OVOFHanConvert.h"
#include "PVTextBuffer.h"

using namespace OpenVanilla;

namespace {

int failures = 0;

void Expect(const std::string& name, const std::vector<std::string>& actual,
            const std::vector<std::string>& expected) {
  if (actual == expected) return;
  ++failures;
  std::cerr << "FAIL " << name << ": got [" << OVStringHelper::Join(actual, "|")
            << "] expected [" << OVStringHelper::Join(expected, "|") << "]\n";
}

std::vector<std::string> CommitWords(const std::string& text,
                                     const std::vector<OVTextBuffer::RangePair>& words) {
  PVTextBuffer buffer;
  buffer.setText(text);
  buffer.setWordSegments(words);
  buffer.commitWordSegments();
  if (!buffer.isEmpty() || !buffer.isCommitted()) {
    ++failures;
    std::cerr << "FAIL commit state for " << text << "\n";
  }
  return buffer.composedCommittedTextSegments();
}

// Mirrors PVLoaderContext's output filter loop.
std::vector<std::string> Run(const std::vector<OVEventHandlingContext*>& filters,
                             std::vector<std::string> segments) {
  for (OVEventHandlingContext* filter : filters) {
    segments = filter->filterTextSegments(segments, nullptr);
  }
  return segments;
}

}  // namespace

int main() {
  const std::vector<std::string> words = {"生活", "著", "名為"};
  std::unique_ptr<OVEventHandlingContext> plain(new OVEventHandlingContext);
  std::unique_ptr<OVEventHandlingContext> simplify(OVOFHanConvert(true).createContext());
  std::unique_ptr<OVEventHandlingContext> fullWidth(OVOFFullWidthCharacter().createContext());

  Expect("default merges", Run({plain.get()}, words), {"生活著名為"});
  Expect("simplify keeps words", Run({simplify.get()}, words), {"生活", "着", "名为"});
  Expect("full width then simplify", Run({fullWidth.get(), simplify.get()}, {"A ", "著", "名為"}),
         {"Ａ　", "着", "名为"});
  Expect("simplify then full width", Run({simplify.get(), fullWidth.get()}, words),
         {"生活", "着", "名为"});
  typedef OVTextBuffer::RangePair Range;
  Expect("commit words", CommitWords("生活著名為", {Range(0, 2), Range(2, 1), Range(3, 2)}), words);
  Expect("commit gaps", CommitWords("生活著名為", {Range(2, 1)}), {"生活", "著", "名為"});
  Expect("commit unmarked", CommitWords("生活著名為", {}), {"生活著名為"});
  Expect("commit overlapping", CommitWords("生活著名為", {Range(0, 3), Range(2, 3)}), {"生活著名為"});
  Expect("commit past end", CommitWords("生活", {Range(0, 5)}), {"生活"});

  if (simplify->filterText("生活著名為", nullptr) != "生活著名为") {
    ++failures;
    std::cerr << "FAIL flat filterText\n";
  }

  if (failures) return 1;
  std::cout << "output filter segments\n";
  return 0;
}
