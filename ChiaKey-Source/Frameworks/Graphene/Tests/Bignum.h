//
// Bignum.h
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

#ifndef Bignum_h
#define Bignum_h

#include <string>

namespace Graphene {
using namespace std;

class BNWrapper {
 public:
  BNWrapper();
  BNWrapper(int i = 0);
  BNWrapper(const BNWrapper&);
  BNWrapper(const string&, size_t base = 10);
  ~BNWrapper();
  BNWrapper& operator=(const BNWrapper&);
  const BNWrapper operator+(const BNWrapper&);
  const BNWrapper operator-(const BNWrapper&);
  const BNWrapper operator*(const BNWrapper&);
  const BNWrapper operator/(const BNWrapper&);
  const BNWrapper operator%(const BNWrapper&);
  bool operator==(const BNWrapper&);
  bool operator<(const BNWrapper&);
  bool operator>(const BNWrapper&);
  bool operator!=(const BNWrapper&);

  const string decString();

 protected:
  BNWrapper(void*);
  void* m_bn;

 protected:
  static void* SharedContext();
  static void DeleteContext();
  static void* c_context;
};

class DecFPN {
 public:
  DecFPN();
  DecFPN(long long i);
  DecFPN(double d);
  DecFPN(const string&) ~DecFPN();
};
};  // namespace Graphene

#endif