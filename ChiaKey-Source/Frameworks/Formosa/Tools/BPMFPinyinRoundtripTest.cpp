// [AUTO_HEADER]

#include <fstream>
#include <iostream>
#include <set>

#include "Mandarin.h"
#ifndef MANDARIN_USE_MINIMAL_OPENVANILLA
#if defined(__APPLE__)
#include <OpenVanilla/OVStringHelper.h>
#else
#include "OVStringHelper.h"
#endif
#else
#include "OVStringHelper.h"
#endif

using namespace std;
using namespace Formosa::Mandarin;
using namespace OpenVanilla;

typedef void (*TestFunc_t)(const string&, size_t);

void TestBPMFToPinyinToBPMF(const string& s, size_t ln) {
  BPMF b = BPMF::FromComposedString(s);
  string p = b.HanyuPinyinString(false, true);
  BPMF nb = BPMF::FromHanyuPinyin(p);

  if (b != nb) {
    cout << "line " << ln << ", B-P-B, original: " << b.composedString()
         << " -> pinyin: " << p << " -> converted: " << nb.composedString()
         << endl;
  }
}

void TestPinyinToBPMFToPinyin(const string& s, size_t ln) {
  BPMF b = BPMF::FromHanyuPinyin(s);

  if (b.HanyuPinyinString(false, true) != s) {
    cout << "line " << ln << ", P-B-P, original: " << s
         << " -> bpmf: " << b.composedString()
         << " -> converted: " << b.HanyuPinyinString(false, true) << endl;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    cerr << "usage: test <unique bpmf list> <unique pinyin list> "
            "<bpmf-to-pinyin ref list>"
         << endl;
    return 1;
  }

  TestFunc_t testFunc[2] = {TestBPMFToPinyinToBPMF, TestPinyinToBPMFToPinyin};

  ifstream ifs;
  string line;

  for (size_t i = 0; i < 2; i++) {
    size_t lineNumber = 0;

    ifs.open(argv[i + 1], ios_base::in);
    if (ifs.fail()) {
      cout << "Failed to open: " << argv[i + 1] << endl;
      continue;
    }

    while (!ifs.eof()) {
      getline(ifs, line);
      lineNumber++;

      if (!line.size()) {
        continue;
      }

      testFunc[i](line, lineNumber);
    }

    ifs.close();
  }

  // third test
  ifs.open(argv[3], ios_base::in);
  if (ifs.fail()) {
    cout << "Failed to open: " << argv[3] << endl;
  } else {
    size_t lineNumber = 0;
    while (!ifs.eof()) {
      getline(ifs, line);
      lineNumber++;

      if (!line.size()) {
        continue;
      }

      vector<string> a = OVStringHelper::SplitBySpacesOrTabs(line);

      BPMF b = BPMF::FromComposedString(a[0]);
      string p = a[1];

      if (b.HanyuPinyinString(false, true) != p) {
        cout << "line: " << lineNumber << ", original: " << line
             << ", B->P fail: " << b.HanyuPinyinString(false, true) << endl;
      }

      if (BPMF::FromHanyuPinyin(p) != b) {
        cout << "line: " << lineNumber << ", original: " << line
             << ", P->B fail: " << BPMF::FromHanyuPinyin(p).composedString()
             << endl;
      }
    }

    ifs.close();
  }

  return 0;
}
