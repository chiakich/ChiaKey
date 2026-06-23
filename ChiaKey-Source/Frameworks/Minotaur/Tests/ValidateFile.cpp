/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include <OpenVanilla/OpenVanilla.h>

#include "Minotaur.h"

using namespace OpenVanilla;
using namespace Minotaur;

int main(int argc, char* argv[]) {
  if (argc < 4) {
    cerr << "ValidateFile file sigFile publicKey" << endl;
    return 1;
  }

  pair<char*, size_t> sigfile = OVFileHelper::SlurpFile(argv[2]);
  pair<char*, size_t> key = OVFileHelper::SlurpFile(argv[3]);

  if (!OVPathHelper::PathExists(argv[1])) {
    cerr << "File doesn't exist: " << argv[1] << endl;
    return 1;
  }

  if (!sigfile.first) {
    cerr << "Cannot slurp " << argv[2] << endl;
    return 1;
  }

  if (!key.first) {
    cerr << "Cannot slurp " << argv[3] << endl;
    return 1;
  }

  if (Minos::ValidateFile(argv[1], sigfile, key))
    cout << "File is good" << endl;
  else
    cout << "File is not good" << endl;

  free(sigfile.first);
  free(key.first);

  return 0;
}
