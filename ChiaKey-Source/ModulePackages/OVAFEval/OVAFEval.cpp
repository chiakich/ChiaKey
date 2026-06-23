//
// OVAFEval.cpp
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

#include "OVAFEval.h"

#ifdef WIN32
#define snprintf sprintf_s
#endif

namespace OpenVanilla {

using namespace Evalgelion;

bool OVAFEvalContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                OVTextBuffer* composingText,
                                OVCandidateService* candidateService,
                                OVLoaderService* loaderService) {
  string locale = loaderService->locale();

  if (key->keyCode() == '9' && key->isCtrlPressed() && !m_enabled &&
      readingText->isEmpty() && composingText->isEmpty() &&
      !loaderService->prompt().size()) {
    m_enabled = true;
    m_expression = "";
    m_cursor = m_expression.begin();

    if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
      loaderService->setPrompt("迷你計算機");
      loaderService->setPromptDescription(
          "請輸入算式，按下 Enter 後開始計算。");
#else
      loaderService->setPrompt(
          "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xa8\x88\xe7\xae\x97\xe6\xa9\x9f");
      loaderService->setPromptDescription(
          "\xE8\xAB\x8B\xE8\xBC\xB8\xE5\x85\xA5\xE7\xAE\x97\xE5\xBC\x8F\xEF\xBC"
          "\x8C\xE6\x8C\x89\xE4\xB8\x8B Enter "
          "\xE5\xBE\x8C\xE9\x96\x8B\xE5\xA7\x8B\xE8\xA8\x88\xE7\xAE\x97\xE3\x80"
          "\x82");
#endif
    } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
      loaderService->setPrompt("迷你计算机");
      loaderService->setPromptDescription(
          "请输入算式，按下 Enter 后开始计算。");

#else
      loaderService->setPrompt(
          "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xae\xa1\xe7\xae\x97\xe6\x9c\xba");
      loaderService->setPromptDescription(
          "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE7\xAE\x97\xE5\xBC\x8F\xEF\xBC"
          "\x8C\xE6\x8C\x89\xE4\xB8\x8B Enter "
          "\xE5\x90\x8E\xE5\xBC\x80\xE5\xA7\x8B\xE8\xAE\xA1\xE7\xAE\x97\xE3\x80"
          "\x82");
#endif
    } else {
      loaderService->setPrompt("Mini Calculator");
      loaderService->setPromptDescription(
          "Please type an expression then enter");
    }
    return true;
  }

  if (!m_enabled) return false;

  if (key->keyCode() == '`') {
    if (!m_interactive) {
      if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
        loaderService->setPrompt("迷你計算機（互動模式）");
        loaderService->setPromptDescription(
            "請輸入算式，按下 Enter 後開始計算。");
#else
        loaderService->setPrompt(
            "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xa8\x88\xe7\xae\x97\xe6\xa9\x9\xEF"
            "\xBC\x88\xE4\xBA\x92\xE5\x8B\x95\xE6\xA8\xA1\xE5\xBC\x8F\xEF\xBC"
            "\x89");
        loaderService->setPromptDescription(
            "\xE8\xAB\x8B\xE8\xBC\xB8\xE5\x85\xA5\xE7\xAE\x97\xE5\xBC\x8F\xEF"
            "\xBC\x8C\xE6\x8C\x89\xE4\xB8\x8B Enter "
            "\xE5\xBE\x8C\xE9\x96\x8B\xE5\xA7\x8B\xE8\xA8\x88\xE7\xAE\x97\xE3"
            "\x80\x82");
#endif
      } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
        loaderService->setPrompt("迷你计算机（互动模式）");
        loaderService->setPromptDescription(
            "请输入算式，按下 Enter 后开始计算。");

#else
        loaderService->setPrompt(
            "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xae\xa1\xe7\xae\x97\xe6\x9c\xba\xEF"
            "\xBC\x88\xE4\xBA\x92\xE5\x8A\xA8\xE6\xA8\xA1\xE5\xBC\x8F\xEF\xBC"
            "\x89");
        loaderService->setPromptDescription(
            "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE7\xAE\x97\xE5\xBC\x8F\xEF"
            "\xBC\x8C\xE6\x8C\x89\xE4\xB8\x8B Enter "
            "\xE5\x90\x8E\xE5\xBC\x80\xE5\xA7\x8B\xE8\xAE\xA1\xE7\xAE\x97\xE3"
            "\x80\x82");
#endif
      } else {
        loaderService->setPrompt("Mini Calculator (Interactive Mode)");
        loaderService->setPromptDescription(
            "Please type an expression then enter");
      }
      m_interactive = true;
    } else {
      if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
        loaderService->setPrompt("迷你計算機");
        loaderService->setPromptDescription(
            "請輸入算式，按下 Enter 後開始計算。");
#else
        loaderService->setPrompt(
            "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xa8\x88\xe7\xae\x97\xe6\xa9\x9");
        loaderService->setPromptDescription(
            "\xE8\xAB\x8B\xE8\xBC\xB8\xE5\x85\xA5\xE7\xAE\x97\xE5\xBC\x8F\xEF"
            "\xBC\x8C\xE6\x8C\x89\xE4\xB8\x8B Enter "
            "\xE5\xBE\x8C\xE9\x96\x8B\xE5\xA7\x8B\xE8\xA8\x88\xE7\xAE\x97\xE3"
            "\x80\x82");
#endif
      } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
        loaderService->setPrompt("迷你计算机");
        loaderService->setPromptDescription(
            "请输入算式，按下 Enter 后开始计算。");

#else
        loaderService->setPrompt(
            "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xae\xa1\xe7\xae\x97\xe6\x9c\xba");
        loaderService->setPromptDescription(
            "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE7\xAE\x97\xE5\xBC\x8F\xEF"
            "\xBC\x8C\xE6\x8C\x89\xE4\xB8\x8B Enter "
            "\xE5\x90\x8E\xE5\xBC\x80\xE5\xA7\x8B\xE8\xAE\xA1\xE7\xAE\x97\xE3"
            "\x80\x82");
#endif
      } else {
        loaderService->setPrompt("Mini Calculator");
        loaderService->setPromptDescription(
            "Please type an expression then enter");
      }
      m_interactive = false;
    }
    return true;
  }

  if (key->keyCode() == OVKeyCode::Esc) {
    readingText->clear();
    readingText->updateDisplay();
    if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
      loaderService->notify("取消使用迷你計算機");
#else
      loaderService->notify(
          "\xE5\x8F\x96\xE6\xB6\x88\xE4\xBD\xBF\xE7\x94\xA8\xE8\xBF\xB7\xE4\xBD"
          "\xA0\xE8\xA8\x88\xE7\xAE\x97\xE6\xA9\x9F");
#endif
    } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
      loaderService->notify("取消使用迷你计算机");
#else
      loaderService->notify(
          "\xE5\x8F\x96\xE6\xB6\x88\xE4\xBD\xBF\xE7\x94\xA8\xE8\xBF\xB7\xE4\xBD"
          "\xA0\xE8\xAE\xA1\xE7\xAE\x97\xE6\x9C\xBA");
#endif
    } else {
      loaderService->notify("Calculator canceled");
    }
    loaderService->setPrompt("");
    loaderService->setPromptDescription("");
    m_enabled = false;
    m_interactive = false;
    m_expression = "";
    m_cursor = m_expression.begin();
    return true;
  }

  if (key->keyCode() == OVKeyCode::Return ||
      key->keyCode() == OVKeyCode::Enter) {
    string result = m_module->m_eval(m_expression);
    if (m_interactive) {
      if (result == "err") {
        composingText->clear();
        if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
          composingText->showToolTip(string("算式輸入錯誤"));
#else
          composingText->showToolTip(
              string("\xE7\xAE\x97\xE5\xBC\x8F\xE8\xBC\xB8\xE5\x85\xA5\xE9\x8C"
                     "\xAF\xE8\xAA\xA4"));
#endif
        } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
          composingText->showToolTip(string("算式输入错误"));
#else
          composingText->showToolTip(
              string("\xE7\xAE\x97\xE5\xBC\x8F\xE8\xBE\x93\xE5\x85\xA5\xE9\x94"
                     "\x99\xE8\xAF\xAF"));
#endif
        } else {
          composingText->showToolTip(string("Error!"));
        }
        composingText->updateDisplay();
        m_expression = "";
      } else {
        m_expression = result;
      }
      readingText->setText(m_expression);
      readingText->setCursorPosition(m_expression.size());
      m_cursor = m_expression.end();
      return true;
    }
    readingText->clear();
    if (result == "err") {
      if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
        composingText->showToolTip(string("算式輸入錯誤"));
#else
        composingText->showToolTip(
            string("\xE7\xAE\x97\xE5\xBC\x8F\xE8\xBC\xB8\xE5\x85\xA5\xE9\x8C"
                   "\xAF\xE8\xAA\xA4"));
#endif
      } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
        composingText->showToolTip(string("算式输入错误"));
#else
        composingText->showToolTip(
            string("\xE7\xAE\x97\xE5\xBC\x8F\xE8\xBE\x93\xE5\x85\xA5\xE9\x94"
                   "\x99\xE8\xAF\xAF"));
#endif
      } else {
        composingText->showToolTip(string("Error!"));
      }
    } else {
      if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
        composingText->showToolTip(string("計算結果： ") + result);
#else
        composingText->showToolTip(string("\xE8\xA8\x88\xE7\xAE\x97\xE7\xB5\x90"
                                          "\xE6\x9E\x9C\xEF\xBC\x9A ") +
                                   result);
#endif
      } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
        composingText->showToolTip(string("计算结果： ") + result);
#else
        composingText->showToolTip(string("\xE8\xAE\xA1\xE7\xAE\x97\xE7\xBB\x93"
                                          "\xE6\x9E\x9C\xEF\xBC\x9A ") +
                                   result);
#endif
      } else {
        composingText->showToolTip(string("Result: ") + result);
      }
    }
    composingText->setText(result);
    composingText->commit();
    loaderService->setPrompt("");
    loaderService->setPromptDescription("");
    m_enabled = false;
    m_expression = "";
    m_cursor = m_expression.begin();
    return true;
  }

  switch (key->keyCode()) {
    case OVKeyCode::Left:
      if (m_cursor == m_expression.begin())
        loaderService->beep();
      else
        --m_cursor;
      break;

    case OVKeyCode::Right:
      if (m_cursor == m_expression.end())
        loaderService->beep();
      else
        ++m_cursor;
      break;

    case OVKeyCode::Delete:
      if (m_cursor == m_expression.end())
        loaderService->beep();
      else
        m_cursor = m_expression.erase(m_cursor);
      break;

    case OVKeyCode::Backspace:
      if (m_cursor == m_expression.begin())
        loaderService->beep();
      else {
        --m_cursor;
        m_cursor = m_expression.erase(m_cursor);
      }
      break;

    default:
      if (!key->receivedString().size())
        loaderService->beep();
      else {
        // string s = key->receivedString();
        // Ignore full-width roman letter
        char buf[2];
        snprintf(buf, 2, "%c", (char)key->keyCode());
        string s = string(buf);
        size_t pos = size_t(m_cursor - m_expression.begin());

        m_expression.insert(m_cursor, s.begin(), s.end());
        m_cursor = m_expression.begin() + pos + 1;
      }
      break;
  }

  readingText->setText(m_expression);
  readingText->setCursorPosition(size_t(m_cursor - m_expression.begin()));
  readingText->updateDisplay();
  return true;
}

const string OVAFEval::localizedName(const string& locale) {
#ifndef _MSC_VER
  string tcname = "迷你計算機";
  string scname = "迷你计算机";
#else
  string tcname =
      "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xa8\x88\xe7\xae\x97\xe6\xa9\x9f";
  string scname =
      "\xe8\xbf\xb7\xe4\xbd\xa0\xe8\xae\xa1\xe7\xae\x97\xe6\x9c\xba";
#endif
  if (locale == "zh_TW" || locale == "zh-Hant")
    return tcname;
  else if (locale == "zh_CN" || locale == "zh-Hans")
    return scname;
  else
    return string("Calculator");
}

const string OVAFEval::identifier() const { return OVAFEVAL_IDENTIFIER; }

};  // namespace OpenVanilla
