//
// Lexer.h
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

#ifndef Lexer_h
#define Lexer_h

#include <iostream>
#include <string>

namespace Graphene {
using namespace std;

class Token {
 public:
  enum TokenType {
    End,
    Integer,
    Float,
    Identifier,
    Operator,
    LeftParenthesis,
    RightParenthesis,
    Assign,
    Dot
  };

  Token(TokenType t = End, string v = "") : type(t), value(v) {}

  operator bool() const { return type != End; }

  TokenType type;
  string value;
};

class Lexer {
 public:
  Lexer(const string& source)
      : m_iterator(source.begin()), m_end(source.end()) {}

  typedef vector<Token> TokenVector;

  static const vector<Token> Parse(const string& source) {
    Lexer lex(source);
    vector<Token> tokens;
    while (Token t = lex.nextToken()) tokens.push_back(t);

    tokens.push_back(Token());
    return tokens;
  }

  Token nextToken() {
    char c, n;
    while (c = current()) {
      ahead();

      if (isalpha(c) || c == '_') {
        return Token(Identifier, identifier(c));
      } else if (isdigit(c)) {
        return number(c);
      }

      switch (c) {
        case '(':
          return Token(LeftParenthesis, "(");

        case ')':
          return Token(RightParenthesis, ")");

        case '.':
          n = current();
          if (n && isdigit(n)) return number(c);
          // return Token(Dot, ".");
          break;

        case '-':
          n = current();
          if (n && isdigit(n)) return number(c);

          if (n && n == '.' && isdigit(peekAhead())) return number(c);

          return Token(Operator, "-");

        case '+':
        case '*':
        case '/':
          return Token(Operator, string(1, c));

        case '=':
          n = current();
          if (n && n == '=') {
            ahead();
            return Token(Operator, "==");
          }

          return Token(Assign, "=");
      }
    }

    return Token(End, string());
  }

 protected:
  string identifier(char typeChar) {
    string result(1, typeChar);

    char c;
    while (c = current()) {
      if (isalpha(c) || isdigit(c)) {
        result += string(1, c);
        ahead();
        continue;
      }

      break;
    }

    return result;
  }

  Token number(char typeChar) {
    bool hasPoint = (typeChar == '.');
    string result(1, typeChar);

    char c;
    while (c = current()) {
      if (isdigit(c)) {
        result += string(1, c);
        ahead();
        continue;
      }

      if (!hasPoint && c == '.') {
        hasPoint = true;
        result += string(1, c);
        ahead();
        continue;
      }

      break;
    }

    return Token(hasPoint ? Float : Integer, result);
  }

  char peekAhead() {
    if (m_iterator == m_end) return 0;

    if (m_iterator + 1 == m_end) return 0;

    return *(m_iterator + 1);
  }

  char peekAheadAhead() {
    if (m_iterator == m_end) return 0;

    if (m_iterator + 1 == m_end) return 0;

    if (m_iterator + 2 == m_end) return 0;

    return *(m_iterator + 2);
  }

  void ahead() {
    if (m_iterator != m_end) ++m_iterator;
  }

  char current() {
    if (m_iterator == m_end) return 0;

    return *m_iterator;
  }

  string::const_iterator m_iterator;
  string::const_iterator m_end;
};

inline ostream& operator<<(ostream& stream, const Lexer::Token& token) {
  string type;
  switch (token.type) {
    case Lexer::End:
      type = "end";
      break;
    case Lexer::Integer:
      type = "integer";
      break;
    case Lexer::Float:
      type = "float";
      break;
    case Lexer::Identifier:
      type = "identifier";
      break;
    case Lexer::Operator:
      type = "operator";
      break;
    case Lexer::LeftParenthesis:
      type = "leftparenthesis";
      break;
    case Lexer::RightParenthesis:
      type = "rightparenthesis";
      break;
    case Lexer::Assign:
      type = "assign";
      break;
    case Lexer::Dot:
      type = "dot";
      break;
  }
  stream << "[" << type << ", '" << token.value << "']";
  return stream;
}

};  // namespace Graphene

#endif