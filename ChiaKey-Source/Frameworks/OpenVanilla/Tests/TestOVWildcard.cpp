// [AUTO_HEADER]

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "UnitTest++.h"

using namespace OpenVanilla;

TEST(WildcardCorrectness) {
  CHECK(OVWildcard::Match("foobarfoobarfoobar123", "*bar?2", '?', '*', false));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "f*"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "*bar"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar123", "*bar123"));
  CHECK(OVWildcard::Match("foobar123foobarfoobar123", "*bar?2*bar?o", '?', '*',
                          false));
  CHECK(OVWildcard::Match("foobarfoobarfoobar123", "*ar*"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "*ar*"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "*ar*ar"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar2", "*ar?"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "f*r"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "f*ar"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "foo*"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "foobar*foobar"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "?o*"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "??*"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "f*bar*oo*a*r"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "f*bar*oo*a*"));
  CHECK(OVWildcard::Match("foobarfoobarfoobar", "f*a??", '?', '*', false));
  CHECK(!OVWildcard::Match("foobarfoobarfoobar", "f*1"));
  CHECK(!OVWildcard::Match("foobarfoobarfoobar", "f*a??"));
}
