//
// TestEvalgelion.cpp
//
// Copyright (c) 2004-2010 The OpenVanilla Project (http://openvanilla.org)
// All rights reserved.
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

#include <cmath>

#include "Evalgelion.h"

using namespace std;
using namespace Evalgelion;

int main() {
  Evaluator eval;

  while (!cin.eof()) {
    string line;
    getline(cin, line);

    cout << eval(line) << endl;

    // vector<Token> tokens = Lexer::Parse(line);
    // for (vector<Token>::const_iterator ti = tokens.begin() ; ti !=
    // tokens.end() ; ++ti)
    //     cout << *ti << endl;

    // Parser parser(tokens);
    // Node* e = parser.expression();
    // if (e) {
    //     cout << endl << "good." << endl;
    //     cout << *e << endl;
    //
    //     double result = e->evaluateWithinContext(c);
    //     double rdresult = round(result);
    //
    //     cout << "result: ";
    //     if (result == rdresult)
    //         cout << (long long)result;
    //     else
    //         cout << fixed << result;
    //     cout << endl;
    //     delete e;
    // }
    // else
    //     cout << endl << "bad." << endl;
  }

  return 0;
}