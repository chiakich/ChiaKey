/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef BPMFUserPhraseHelper_h
#define BPMFUserPhraseHelper_h

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace Manjusri {
using namespace OpenVanilla;

class BPMFUserPhraseHelper {
 public:
  static bool Import(OVSQLiteConnection* db, const string& filename);
  static bool Export(OVSQLiteConnection* db, const string& filename);
  static OVSQLiteConnection* OpenUserPhraseDB(OVPathInfo* pathInfo,
                                              OVLoaderService* loaderService);

  static const pair<string, size_t> QString(const string& bpmfString);

  static const string BPMFString(const string& absString);
};
};  // namespace Manjusri

#endif
