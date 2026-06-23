// [AUTO_HEADER]

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "UnitTest++.h"

using namespace OpenVanilla;

TEST(StringHelper) {
  string test1 = "a b\tc    d    ";
  string test2 = "  a b\tc    d    ";
  string test3 = "  a b\tc    d";
  vector<string> source;
  source.push_back("a");
  source.push_back("b");
  source.push_back("c");
  source.push_back("d");

  vector<string> data;
  data = OVStringHelper::SplitBySpacesOrTabs(test1);
  CHECK(data == source);
  data = OVStringHelper::SplitBySpacesOrTabs(test2);
  CHECK(data == source);
  data = OVStringHelper::SplitBySpacesOrTabs(test3);
  CHECK(data == source);

  source.push_back("");
  source.push_back("e");
  string test4 = "a-b-c-d--e";
  data = OVStringHelper::Split(test4, '-');
  CHECK(data == source);
}
