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

void TestBPMFToPHTToBPMF(const string& s, size_t ln) {
  BPMF b = BPMF::FromComposedString(s);
  string p = b.PHTString(false);
  BPMF nb = BPMF::FromPHT(p);

  if (b != nb) {
    cerr << "line " << ln << ", B-P-B, original: " << b.composedString()
         << " -> pht: " << p << " -> converted: " << nb.composedString()
         << endl;
  } else {
    cout << p << "\t" << b.HanyuPinyinString(false, true) << "\t"
         << b.composedString() << endl;
  }
}

void TestPHTToBPMFToPHT(const string& s, size_t ln) {
  BPMF b = BPMF::FromPHT(s);

  if (b.PHTString(false) != s) {
    cout << "line " << ln << ", P-B-P, original: " << s
         << " -> bpmf: " << b.composedString()
         << " -> converted: " << b.PHTString(false) << endl;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    cerr << "usage: test <unique bpmf list> <unique pht list> <bpmf-to-pht ref "
            "list>"
         << endl;
    return 1;
  }

  TestFunc_t testFunc[2] = {TestBPMFToPHTToBPMF, TestPHTToBPMFToPHT};

  ifstream ifs;
  string line;

  for (size_t i = 0; i < 1; i++) {
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

  return 0;

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

      if (b.PHTString(false) != p) {
        cout << "line: " << lineNumber << ", original: " << line
             << ", B->P fail: " << b.PHTString(false) << endl;
      }

      if (BPMF::FromPHT(p) != b) {
        cout << "line: " << lineNumber << ", original: " << line
             << ", P->B fail: " << BPMF::FromPHT(p).composedString() << endl;
      }
    }

    ifs.close();
  }

  return 0;
}
