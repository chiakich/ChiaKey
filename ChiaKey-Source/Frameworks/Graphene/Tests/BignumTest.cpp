//
// BignumTest.cpp
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

#include <openssl/bn.h>
#include <openssl/crypto.h>

#include <iostream>
#include <sstream>

#include "Bignum.h"

#define M_BN ((BIGNUM*)m_bn)
#define O_BN ((BIGNUM*)other.m_bn)
#define INIT_N  \
  BIGNUM* n;    \
  n = BN_new(); \
  BN_init(n)
#define CTX ((BN_CTX*)SharedContext())

namespace Graphene {

void* BNWrapper::c_context = 0;

void* BNWrapper::SharedContext() {
  if (c_context) return c_context;

  c_context = BN_CTX_new();
  BN_CTX_init((BN_CTX*)c_context);
}

void BNWrapper::DeleteContext() {
  if (c_context) {
    BN_CTX_free((BN_CTX*)c_context);
    c_context = 0;
  }
}

BNWrapper::BNWrapper() : m_bn(0) {
  m_bn = BN_new();
  BN_init(M_BN);
}

BNWrapper::BNWrapper(int i) : m_bn(0) {
  stringstream sst;
  sst << i;

  BIGNUM* result = 0;
  BN_dec2bn(&result, sst.str().c_str());
  m_bn = result;

  // @todo: handles error
}

BNWrapper::BNWrapper(const BNWrapper& other) : m_bn(0) { m_bn = BN_dup(O_BN); }

BNWrapper::BNWrapper(const string& str, size_t base) {
  BIGNUM* result = 0;

  switch (base) {
    case 10:
      BN_dec2bn(&result, str.c_str());
      m_bn = result;

      // @todo: handles error
      return;
    default:
      // @todo: handles error
      break;
  }
}

BNWrapper::BNWrapper(void* bnPtr) : m_bn(bnPtr) {}

BNWrapper::~BNWrapper() { BN_free(M_BN); }

BNWrapper& BNWrapper::operator=(const BNWrapper& other) {
  BN_free(M_BN);
  m_bn = BN_dup(O_BN);
  return *this;
}

const BNWrapper BNWrapper::operator+(const BNWrapper& other) {
  INIT_N;
  BN_add(n, M_BN, O_BN);
  return BNWrapper(n);
}

const BNWrapper BNWrapper::operator-(const BNWrapper& other) {
  INIT_N;
  BN_sub(n, M_BN, O_BN);
  return BNWrapper(n);
}

const BNWrapper BNWrapper::operator*(const BNWrapper& other) {
  INIT_N;
  BN_mul(n, M_BN, O_BN, CTX);
  return BNWrapper(n);
}

const BNWrapper BNWrapper::operator/(const BNWrapper& other) {
  INIT_N;
  BN_div(n, NULL, M_BN, O_BN, CTX);
  return BNWrapper(n);
}

const BNWrapper BNWrapper::operator%(const BNWrapper& other) {
  INIT_N;
  BN_mod(n, M_BN, O_BN, CTX);
  return BNWrapper(n);
}

bool BNWrapper::operator==(const BNWrapper& other) {
  return !BN_cmp(M_BN, O_BN);
}

bool BNWrapper::operator<(const BNWrapper& other) {
  return BN_cmp(M_BN, O_BN) < 0;
}

bool BNWrapper::operator>(const BNWrapper& other) {
  return BN_cmp(M_BN, O_BN) > 0;
}

bool BNWrapper::operator!=(const BNWrapper& other) {
  return !!BN_cmp(M_BN, O_BN);
}

const string BNWrapper::decString() {
  char* decStr = BN_bn2dec(M_BN);
  string result(decStr);
  OPENSSL_free(decStr);
  return result;
}

};  // namespace Graphene

using namespace Graphene;
using namespace std;

int main() {
  cerr << "a" << endl;
  BNWrapper a(100);
  cerr << "b" << endl;
  BNWrapper b("10000000000000");
  cerr << "c" << endl;
  BNWrapper c = a + b;
  cerr << "o" << endl;
  cout << c.decString() << endl;

  BNWrapper d = c / BNWrapper(2);
  cout << d.decString() << endl;

  return 0;
}
