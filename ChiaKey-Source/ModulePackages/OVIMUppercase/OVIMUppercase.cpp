//
// OVIMUppercase.cpp
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

#include "OVIMUppercase.h"

#include <ctype.h>

namespace OpenVanilla {

OVIMUppercaseContext::OVIMUppercaseContext() {}

void OVIMUppercaseContext::startSession(OVLoaderService* loaderService) {
  loaderService->logger("OVIMUppercaseContext") << "session started" << endl;

  m_strings.clear();
}

void OVIMUppercaseContext::stopSession(OVLoaderService* loaderService) {
  loaderService->logger("OVIMUppercaseContext") << "session ended" << endl;
}

bool OVIMUppercaseContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                     OVTextBuffer* composingText,
                                     OVCandidateService* candidateService,
                                     OVLoaderService* loaderService) {
  loaderService->logger("OVIMUppercaseContext")
      << "received key, code = " << key->keyCode()
      << ", string = " << key->receivedString() << endl;

  // if it's non-printable key and the composing text is empty, we return the
  // event
  if (!key->receivedString().size() && composingText->isEmpty()) return false;

  // so, if we're here, only two conditions are possible:
  // 1. composing text is not empty and it's a non-printable key (return,
  // backspace, escape, and everything else)
  // 2. it's a printable key (regardless the composing text state)

  if (key->keyCode() == OVKeyCode::Left) {
    loaderService->logger("left")
        << "cursor = " << composingText->cursorPosition() << endl;
    if (composingText->cursorPosition())
      composingText->setCursorPosition(composingText->cursorPosition() - 1);

    return true;
  }

  if (key->keyCode() == OVKeyCode::Right) {
    loaderService->logger("right")
        << "cursor = " << composingText->cursorPosition() << endl;

    if (composingText->cursorPosition() != composingText->codePointCount())
      composingText->setCursorPosition(composingText->cursorPosition() + 1);

    return true;
  }

  OVOneDimensionalCandidatePanel* panel =
      candidateService->useVerticalCandidatePanel();

  if (key->receivedString().size()) {
    if (key->receivedString() == "5") {
      if (composingText->codePointCount() >= 3) {
        composingText->setHighlightMark(
            OVTextBuffer::RangePair(1, composingText->codePointCount() - 2));
      }

      return true;
    }

    if (key->receivedString() == "6") {
      if (composingText->codePointCount() >= 6) {
        vector<OVTextBuffer::RangePair> segs;
        segs.push_back(OVTextBuffer::RangePair(0, 1));
        segs.push_back(OVTextBuffer::RangePair(1, 2));
        composingText->setHighlightMark(OVTextBuffer::RangePair(3, 2));
        segs.push_back(
            OVTextBuffer::RangePair(5, composingText->codePointCount() - 5));
        composingText->setWordSegments(segs);
      }

      return true;
    }

    if (key->receivedString() == "1" && !panel->isVisible()) {
      OVCandidateList* list = panel->candidateList();
      list->addCandidate("foo");
      list->addCandidate("bar");
      list->addCandidate("blah1");
      list->addCandidate("blah2");
      list->addCandidate("blah3");
      list->addCandidate("blah4");
      list->addCandidate("blah5");
      list->addCandidate("blah6");
      list->addCandidate("blah7");
      list->addCandidate("blah8");
      list->addCandidate("blah9");
      list->addCandidate("foolar");

      panel->updateDisplay();
      panel->show();
    }

    if (key->receivedString() == "2" && panel->isVisible()) {
      panel->yieldToCandidateEventHandler();
      return true;
    }

    if (key->receivedString() == "3") {
      OVPlainTextCandidatePanel* textPanel =
          candidateService->usePlainTextCandidatePanel();
      textPanel->textStorage()->setContent("hello, world");
      textPanel->show();
      return true;
    }

    if (key->receivedString() == "4") {
      OVPlainTextCandidatePanel* textPanel =
          candidateService->usePlainTextCandidatePanel();
      textPanel->hide();
      return true;
    }

    m_strings.push_back(key->receivedString());
  } else if (key->keyCode() == OVKeyCode::Return) {
    composingText->setText(combineString(true));
    composingText->commit();
    m_strings.clear();
    candidateService->useVerticalCandidatePanel()->hide();
    return true;
  } else if (key->keyCode() == OVKeyCode::Esc) {
    m_strings.clear();
    composingText->clear();
    composingText->updateDisplay();
    return true;
  } else if (key->keyCode() == OVKeyCode::Backspace) {
    m_strings.pop_back();
  } else {
    loaderService->beep();
  }

  composingText->setText(combineString());
  composingText->updateDisplay();

  return true;
}

const string OVIMUppercaseContext::combineString(bool convert) {
  string result;
  vector<string>::iterator iter = m_strings.begin();
  for (; iter != m_strings.end(); ++iter) {
    if (convert) {
      string::iterator si = (*iter).begin();
      for (; si != (*iter).end(); ++si) result += string(1, toupper(*si));
    } else {
      result += *iter;
    }
  }

  return result;
}

OVEventHandlingContext* OVIMUppercase::createContext() {
  return new OVIMUppercaseContext;
}

const string OVIMUppercase::identifier() const {
  return string("OVIMUppercase");
}

const string OVIMUppercase::localizedName(const string& locale) {
  return string("Uppercase Input Method");
}

bool OVIMUppercase::initialize(OVPathInfo* pathInfo,
                               OVLoaderService* loaderService) {
  loaderService->logger("OVIMUppercase") << "Initializing" << endl;

  vector<OVDatabaseService*> services;
  services.push_back(loaderService->CINDatabaseService());
  services.push_back(loaderService->SQLiteDatabaseService());
  for (size_t index = 0; index < 2; index++) {
    loaderService->logger("OVIMUppercase")
        << "service index: " << index << endl;

    OVDatabaseService* service = services[index];
    if (service) {
      vector<string> tables = service->tables();
      for (size_t j = 0; j < tables.size(); j++)
        loaderService->logger("OVIMUppercase")
            << "  table: " << tables[j] << endl;
    }
  }

  return true;
}

void OVIMUppercase::finalize() {}

void OVIMUppercase::loadConfig(OVKeyValueMap* moduleConfig,
                               OVLoaderService* loaderService) {
  loaderService->logger("OVIMUppercase") << "Load config" << endl;
}

void OVIMUppercase::saveConfig(OVKeyValueMap* moduleConfig,
                               OVLoaderService* loaderService) {
  loaderService->logger("OVIMUppercase") << "Save config" << endl;
  moduleConfig->setKeyStringValue("use", "test");
}
};  // namespace OpenVanilla
