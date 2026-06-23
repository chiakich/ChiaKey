//
// Evalgelion.h
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

#ifndef Evalgelion_h
#define Evalgelion_h

#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef WIN32
#include <float.h>
#define isnan _isnan
#define INFINITY INF
#define round floor
#endif

namespace Evalgelion {
using namespace std;

using namespace std;

class Token {
 public:
  enum Type {
    End,
    Number,
    Identifier,
    Plus,
    Negate,
    Multiply,
    Divide,
    Equal,
    LeftParenthesis,
    RightParenthesis,
    Assign,
    Dot,
    Unknown
  };

  Token(Type t = End, string v = "") : type(t), value(v) {}

  operator bool() const { return type != End; }

  Type type;
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

      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
        return Token(Token::Identifier, identifier(c));
      } else if (c >= '0' && c <= '9') {
        return number(c);
      }

      switch (c) {
        case '(':
          return Token(Token::LeftParenthesis, "(");

        case ')':
          return Token(Token::RightParenthesis, ")");

        case '.':
          n = current();
          if (n && (n >= '0' && n <= '9')) return number(c);
          // return Token(Dot, ".");
          break;

        case '-':
          return Token(Token::Negate, "-");

        case '+':
          return Token(Token::Plus, string(1, c));

        case '*':
          return Token(Token::Multiply, string(1, c));

        case '/':
          return Token(Token::Divide, string(1, c));

        case '=':
          n = current();
          if (n && n == '=') {
            ahead();
            return Token(Token::Equal, "==");
          }

          return Token(Token::Assign, "=");

        case ' ':
        case '\t':
          break;

        default:
          return Token(Token::Unknown, string(1, c));
      }
    }

    return Token(Token::End, string());
  }

 protected:
  string identifier(char typeChar) {
    string result(1, typeChar);

    char c;
    while (c = current()) {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9')) {
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
      if (c >= '0' && c <= '9') {
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

    // return Token(hasPoint ? Token::Float : Token::Integer, result);
    return Token(Token::Number, result);
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

inline ostream& operator<<(ostream& stream, const Token& token) {
  string type;
  switch (token.type) {
    case Token::End:
      type = "end";
      break;
    case Token::Number:
      type = "number";
      break;
    case Token::Identifier:
      type = "identifier";
      break;
    case Token::Negate:
    case Token::Plus:
    case Token::Multiply:
    case Token::Divide:
    case Token::Equal:
      type = "operator";
      break;
    case Token::LeftParenthesis:
      type = "leftparenthesis";
      break;
    case Token::RightParenthesis:
      type = "rightparenthesis";
      break;
    case Token::Assign:
      type = "assign";
      break;
    case Token::Dot:
      type = "dot";
      break;
  }
  stream << "[" << type << ", '" << token.value << "']";
  return stream;
}

typedef double Value;

class Context {
 public:
  double value(const string& key) { return m_variables[key]; }

  double setValue(const string& key, Value value) {
    m_variables[key] = value;
    return value;
  }

  void clear() { m_variables.clear(); }

 protected:
  map<string, Value> m_variables;
};

class Node {
 public:
  Node(const Token& op = Token(), Node* left = 0, Node* right = 0)
      : m_op(op), m_left(left), m_right(right) {}

  ~Node() {
    if (m_left) delete m_left;

    if (m_right) delete m_right;
  }

  Value evaluateWithinContext(Context& context) {
    switch (m_op.type) {
      case Token::Number:
        return atof(m_op.value.c_str());

      case Token::Identifier:
        if (m_right) {
          if (m_op.value == "sin") {
            return sin(m_right->evaluateWithinContext(context));
          } else if (m_op.value == "cos") {
            return cos(m_right->evaluateWithinContext(context));
          } else if (m_op.value == "abs") {
            return abs(m_right->evaluateWithinContext(context));
          }
        }

        return context.value(m_op.value);

      case Token::Plus:
        if (m_left && m_right)
          return m_left->evaluateWithinContext(context) +
                 m_right->evaluateWithinContext(context);
        return (Value)0;

      case Token::Negate:
        if (!m_right) return -(m_left->evaluateWithinContext(context));
        if (m_left && m_right)
          return m_left->evaluateWithinContext(context) -
                 m_right->evaluateWithinContext(context);
        return (Value)0;

      case Token::Multiply:
        if (m_left && m_right)
          return m_left->evaluateWithinContext(context) *
                 m_right->evaluateWithinContext(context);
        return (Value)0;

      case Token::Divide:
        if (m_left && m_right)
          return m_left->evaluateWithinContext(context) /
                 m_right->evaluateWithinContext(context);
        return (Value)0;

      case Token::Equal:
        if (m_left && m_right)
          return m_left->evaluateWithinContext(context) ==
                 m_right->evaluateWithinContext(context);
        return (Value)0;

      case Token::Assign:
        if (m_left && m_right)
          return context.setValue(m_left->m_op.value,
                                  m_right->evaluateWithinContext(context));
        return (Value)0;

      default:
        return (Value)0;
    }
  }

  friend ostream& operator<<(ostream& stream, const Node& node);

 protected:
  Token m_op;
  Node* m_left;
  Node* m_right;
};

inline ostream& operator<<(ostream& stream, const Node& node) {
  stream << "<" << node.m_op << ", ";
  if (node.m_left)
    stream << *(node.m_left);
  else
    stream << "LNIL";

  stream << ", ";

  if (node.m_right)
    stream << *(node.m_right);
  else
    stream << "RNIL";

  stream << ">";

  return stream;
}

class Parser {
 public:
  Parser(const vector<Token>& tokens)
      : m_tokenIterator(tokens.begin()), m_tokenEnd(tokens.end()) {}

  // expression :=    identifer ASSIGN expression
  //                  subexp + expression
  //                  subexp - expression
  // subexp :=        entity
  //                  entity * subexp
  //                  entity / subexp
  // entity :=        identifier
  //                  number
  //                  negate identifier
  //                  negate number
  //                  negate ( expression )
  //                  ( expression )

  Node* subexp() {
    Token current = currentToken();

    // cout << "subexp ";
    // cout << current << "; ";

    Node* e;

    switch (current.type) {
      case Token::Identifier:
      case Token::Number:
      case Token::Negate:
      case Token::LeftParenthesis:
        // cout << " [ ";
        e = entity();
        if (!e) return 0;
        break;

      default:
        return 0;
    }

    // cout << " -- subexp' ";
    while (1) {
      Node* ne;
      Token next = currentToken();
      // cout << next << "; ";
      if (next.type != Token::Multiply && next.type != Token::Divide) break;

      // cout << next.value;
      moveAhead();

      current = currentToken();
      switch (current.type) {
        case Token::Identifier:
        case Token::Number:
        case Token::Negate:
        case Token::LeftParenthesis:
          ne = entity();
          if (!ne) {
            delete e;
            return 0;
          }

          e = new Node(next, e, ne);

          // cout << " ] ";
          break;

        default:
          return 0;
      }
    }

    // cout << " -- subexp'(stop) ";
    return e;
  }

  Node* entity() {
    Token current = currentToken();
    Token next = peekAhead();
    Node* ne;

    // cout << "entity ";
    // cout << current;
    // cout << "; ";

    if (current.type == Token::Identifier &&
        next.type == Token::LeftParenthesis) {
      // cout << "identifier function" << endl;
      moveAhead();
      // moveAhead();
      // cout << current.value << "(";

      Node* right = entity();
      if (!right) {
        delete ne;
        return 0;
      }

      // ignore the )
      // moveAhead();

      return new Node(current, 0, right);
    }

    switch (current.type) {
      case Token::Identifier:
        // cout << current.value;
        moveAhead();
        return new Node(current);

      case Token::Number:
        // cout << current.value;
        moveAhead();
        return new Node(current);

      case Token::Negate:
        moveAhead();
        // cout << " <neg ";
        ne = entity();
        if (!ne) return 0;
        return new Node(current, ne);

      case Token::LeftParenthesis:
        // cout << "(";
        moveAhead();

        ne = expression();
        if (!ne) return 0;

        next = currentToken();
        if (next.type != Token::RightParenthesis) {
          delete ne;
          return 0;
        }

        moveAhead();
        // cout << ")";
        return ne;
    }

    return nullptr;
  }

  Node* expression() {
    Token current = currentToken();
    Token next = peekAhead();
    Node* ne;

    // cout << "expression " << current << "; ";

    if (current.type == Token::Identifier && next.type == Token::Assign) {
      moveAhead();
      moveAhead();
      // cout << current.value << " = ";

      Node* right = expression();
      if (!right) return 0;

      return new Node(next, new Node(current), right);
    } else if (ne = subexp()) {
      while (1) {
        // cout << "looping" << endl;

        next = currentToken();
        if (next.type == Token::Plus || next.type == Token::Negate ||
            next.type == Token::Equal) {
          // cout << "plus/negate" << endl;
          // cout << next.value;
          moveAhead();

          Token op = next;

          current = currentToken();
          next = peekAhead();

          Node* right;
          if (current.type == Token::Identifier && next.type == Token::Assign) {
            // cout << "assign within exp" << endl;
            moveAhead();
            moveAhead();
            // cout << current.value << " = ";

            right = expression();
            if (!right) {
              delete ne;
              return 0;
            }

            right = new Node(next, new Node(current), right);
          } else {
            // cout << "eval subexp" << endl;
            right = subexp();
            if (!right) {
              if (ne) delete ne;
              return 0;
            }
          }

          ne = new Node(op, ne, right);
          // cout << "NE = " << *ne << endl;
        } else if (next.type == Token::End ||
                   next.type == Token::RightParenthesis) {
          // cout << "end of token" << endl;
          // cout << "NE = " << *ne << endl;
          return ne;
        } else {
          // cout << "invalid" << endl;
          // cout << "NE = " << *ne << endl;

          if (ne) delete ne;

          return 0;
        }
      }
    } else if (!current) {
      // cout << "end";
      return new Node;
    } else {
      return 0;
    }
  }

 protected:
  Token currentToken() { return *m_tokenIterator; }

  Token peekAhead() {
    if (m_tokenIterator + 1 == m_tokenEnd) return Token();

    return *(m_tokenIterator + 1);
  }

  void moveAhead() {
    if (m_tokenIterator != m_tokenEnd) ++m_tokenIterator;
  }

  vector<Token>::const_iterator m_tokenIterator;
  vector<Token>::const_iterator m_tokenEnd;
};

class Evaluator {
 public:
  const string operator()(const string& expression) {
    if (expression == "clear") {
      m_context.clear();
      return string();
    }

    vector<Token> tokens = Lexer::Parse(expression);
    Parser parser(tokens);
    Node* e = parser.expression();
    if (e) {
      stringstream sst;

      double result = e->evaluateWithinContext(m_context);

#ifndef WIN32
      if (result == INFINITY) return string("err");
#else
      if (!_finite(result)) return string("err");
#endif
      if (isnan(result)) return string("err");

      double rdresult = round(result);

      if (result == rdresult)
        sst << (long long)result;
      else
        sst << fixed << result;

      delete e;
      return sst.str();
    }

    return string("err");
  }

 protected:
  Context m_context;
};

};  // namespace Evalgelion

#endif
