#include <iostream>
#include <memory>

#include "OVOFFullWidthCharacter.h"
#include "OVOFHanConvert.h"

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
  if (simplify->filterText("生活著名為", nullptr) != "生活著名为") {
    ++failures;
    std::cerr << "FAIL flat filterText\n";
  }

  if (failures) return 1;
  std::cout << "output filter segments\n";
  return 0;
}
