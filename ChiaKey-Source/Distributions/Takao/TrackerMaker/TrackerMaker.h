// [AUTO_HEADER]

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#include <PlainVanilla/PVModuleSystem.h>
#else
#include "LFPlatform.h"
#include "OpenVanilla.h"
#include "PVModuleSystem.h"
#endif

#include <sstream>

namespace Takao {
using namespace OpenVanilla;

class TrackerMaker {
 public:
  TrackerMaker() {
#if defined(__APPLE__)
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
    m_platform = "M105";
#else
    m_platform = "M104";
#endif
#elif defined(WIN32)
    m_platform =
        LFPlatform::ProcessHelper::IsRunningUnderWow64() ? "W64" : "W32";
#else
    m_platform = "Unknown";
#endif

    m_takaoBuildNumber = "2528";

    srand((unsigned int)time(NULL));
  }

  // onekey-<component>
  const string trackerURLString(const string& trackerAndComponent) {
    if (OVWildcard::Match(trackerAndComponent, "onekey-*")) {
      return onekeyTrackerURLString(trackerAndComponent.substr(7));
    } else if (OVWildcard::Match(trackerAndComponent, "LoaderStart")) {
      return loaderStartURLString();
    }

    return string();
  }

  // dictionary, search, stock, wretch, auction, map
  const string onekeyTrackerURLString(const string& component) {
    string platformAndVersion = m_takaoBuildNumber + m_platform;
    string baseURLString =
        "http://tw.rd.yahoo.com/referurl/keykey/client/onekey/{COMPONENT}/"
        "{PLATFORM_AND_VERSION}/*http://tw.yimg.com/i/tw/download/ykkimg/"
        "blank.gif?rand=";

    string result;
    result = OVStringHelper::StringByReplacingOccurrencesOfStringWithString(
        baseURLString, "{COMPONENT}", component);
    result = OVStringHelper::StringByReplacingOccurrencesOfStringWithString(
        result, "{PLATFORM_AND_VERSION}", platformAndVersion);

    result += randomNumberString();

    return result;
  }

  const string loaderStartURLString() {
    string platformAndVersion = m_takaoBuildNumber + m_platform;
    string baseURLString =
        "http://tw.rd.yahoo.com/referurl/keykey/client/keykey/start/"
        "{PLATFORM_AND_VERSION}/*http://tw.yimg.com/i/tw/download/ykkimg/"
        "blank.gif?rand=";

    string result;
    result = OVStringHelper::StringByReplacingOccurrencesOfStringWithString(
        baseURLString, "{PLATFORM_AND_VERSION}", platformAndVersion);

    result += randomNumberString();

    return result;
  }

 protected:
  const string randomNumberString() {
    stringstream sst;
#ifdef WIN32
    sst << rand() << rand();
#else
    sst << random();
#endif

    return sst.str();
  }

  string m_platform;
  string m_takaoBuildNumber;
};

};  // namespace Takao
