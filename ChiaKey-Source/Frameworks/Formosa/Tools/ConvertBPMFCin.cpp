// [AUTO_HEADER]

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Mandarin.h"
#include "OVBenchmark.h"
#include "OVStringHelper.h"
#include "OVUTF8Helper.h"
#include "OVWildcard.h"

using namespace OpenVanilla;
using namespace Formosa::Mandarin;
using namespace std;

int main() {
  OVWildcard chardef("%chardef", '?', '*', false);

  string line;
  while (!cin.eof()) {
    getline(cin, line);
    cout << line << endl;

    if (chardef.match(line)) {
      while (!cin.eof()) {
        getline(cin, line);
        if (chardef.match(line)) {
          cout << line << endl;
          break;
        }

        vector<string> vec = OVStringHelper::SplitBySpacesOrTabs(line);
        const BopomofoKeyboardLayout* layout =
            BopomofoKeyboardLayout::StandardLayout();

        BPMF bpmf = layout->syllableFromKeySequence(vec[0]);
        cout << bpmf.absoluteOrderString() << " " << vec[1] << endl;
      }
    }
  }

  return 0;
}