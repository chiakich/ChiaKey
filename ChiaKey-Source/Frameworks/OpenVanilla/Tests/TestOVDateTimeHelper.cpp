// [AUTO_HEADER]

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "UnitTest++.h"

using namespace OpenVanilla;

TEST(DateTimeHelper) {
  cout << OVDateTimeHelper::LocalDateTimeString() << endl;
  CHECK(OVDateTimeHelper::LocalTimeString().length());
  CHECK(OVDateTimeHelper::LocalDateTimeString().length());
}
