// [AUTO_HEADER]

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "UnitTest++.h"

using namespace OpenVanilla;

TEST(OpenVanillaVersion) { CHECK_EQUAL(1, OVFrameworkInfo::MajorVersion()); }
