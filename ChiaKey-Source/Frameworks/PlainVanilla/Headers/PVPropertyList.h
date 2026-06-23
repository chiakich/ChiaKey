//
// PVPropertyList.h
//
// Copyright (c) 2007-2010 Lukhnos D. Liu (http://lukhnos.org)
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef PVPropertyList_h
#define PVPropertyList_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#include <PlainVanilla/PVPlistValue.h>
#else
#include "OpenVanilla.h"
#include "PVPlistValue.h"
#endif

namespace OpenVanilla {
using namespace std;

class PVPropertyList {
 public:
  PVPropertyList(const string& filename, bool syncAtStart = true)
      : m_filename(filename), m_mirroredRootDictionary(0) {
    m_timestamp = OVPathHelper::TimestampForPath(filename);
    m_rootDictionary = new PVPlistValue(PVPlistValue::Dictionary);
    m_mirroredRootDictionary = m_rootDictionary->copy();

    if (syncAtStart) readSync(true);
  }

  ~PVPropertyList() {
    if (m_rootDictionary) delete m_rootDictionary;

    if (m_mirroredRootDictionary) delete m_mirroredRootDictionary;
  }

  bool shouldReadSync() {
    OVFileTimestamp timestamp = OVPathHelper::TimestampForPath(m_filename);
    return timestamp > m_timestamp;
  }

  void forceSyncForNextRound() { m_timestamp = OVFileTimestamp(); }

  void readSync(bool forced = false) {
    if (!forced)
      if (!shouldReadSync()) return;

    PVPlistValue* tmp = m_rootDictionary;
    PVPlistValue* newDictionary = ParsePlist(m_filename);

    if (m_mirroredRootDictionary) delete m_mirroredRootDictionary;

    if (newDictionary) {
      m_rootDictionary = newDictionary;
      delete tmp;
    }

    m_mirroredRootDictionary = m_rootDictionary->copy();
    m_timestamp = OVPathHelper::TimestampForPath(m_filename);
  }

  void write() {
    if (m_rootDictionary && m_mirroredRootDictionary)
      if (m_rootDictionary == m_mirroredRootDictionary) return;

    string directory = OVPathHelper::DirectoryFromPath(m_filename);
    OVDirectoryHelper::MakeDirectoryWithImmediates(directory);

    WritePlist(m_filename, m_rootDictionary);
    m_timestamp = OVPathHelper::TimestampForPath(m_filename);

    if (m_mirroredRootDictionary) delete m_mirroredRootDictionary;

    m_mirroredRootDictionary = m_rootDictionary->copy();
  }

  PVPlistValue* rootDictionary() { return m_rootDictionary; }

  void setRootDictionary(PVPlistValue* dictionary, bool useCopy = true) {
    if (m_rootDictionary == dictionary) return;

    PVPlistValue* tmp = m_rootDictionary;
    if (useCopy) {
      m_rootDictionary = dictionary->copy();
    } else {
      m_rootDictionary = dictionary;
    }

    delete tmp;
  }

  static PVPlistValue* ParsePlistFromString(const char* stringData);

 protected:
  static PVPlistValue* ParsePlist(const string& filename);
  static void WritePlist(const string& filename, PVPlistValue* rootDictionary);

  OVFileTimestamp m_timestamp;
  string m_filename;
  PVPlistValue* m_rootDictionary;
  PVPlistValue* m_mirroredRootDictionary;

 public:
#if defined(__APPLE__)
  // for facilitating C++-only tool
  static void CreateNSAutoreleasePoolInMain();
#endif
};
};  // namespace OpenVanilla

#endif
