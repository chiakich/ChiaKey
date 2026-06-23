//
// OVIMSmartMandarin.cpp
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

#include "OVIMSmartMandarin.h"

#include "OVIMMandarinKeyUtils.h"
#include "OVIMMandarinConfig.h"

//#ifdef OVIMSMARTMANDARIN_USE_SQLITE_CRYPTO
// using namespace std;
// pair<char*, size_t> ObtenirUserDonneCle();
// int sqlite3_rekey(sqlite3 *db, const void *pKey, int nKey);
//#endif

namespace OpenVanilla {

class OVIMSmartMandarinStringFilter : public StringFilter {
 public:
  OVIMSmartMandarinStringFilter(const string& encoding,
                                OVEncodingService* encodingService)
      : m_encoding(encoding), m_encodingService(encodingService) {}

  virtual bool shouldPass(const string& text) {
    if (!m_encodingService) return true;

    return m_encodingService->stringSupportedByEncoding(text, m_encoding);
  }

 protected:
  string m_encoding;
  OVEncodingService* m_encodingService;
};

OVIMSmartMandarinContext::OVIMSmartMandarinContext(OVIMSmartMandarin* module)
    : m_module(module),
      m_manjusri(module->m_LM),
      m_cursor(0),
      m_BPMFReading(0),
      m_markMode(false),
      m_markCursor(true) {}

const BopomofoKeyboardLayout*
OVIMSmartMandarinContext::currentKeyboardLayout() {
  const BopomofoKeyboardLayout* layout =
      BopomofoKeyboardLayout::LayoutForName(m_module->m_cfgKeyboardLayout);

  if (!layout) layout = BopomofoKeyboardLayout::StandardLayout();

  return layout;
}

void OVIMSmartMandarinContext::startSession(OVLoaderService* loaderService) {
  m_BPMFReading.setKeyboardLayout(currentKeyboardLayout());
  m_BPMFReading.clear();
  m_manjusri.clear();
  m_cursor = m_manjusri.cursorLeftBound();
  m_markMode = false;
  m_markCursor = m_cursor;
}

void OVIMSmartMandarinContext::stopSession(OVLoaderService* loaderService) {
  OVBenchmark bench;
  bench.start();

  //    loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Saves user
  //    bigram/candidate override caches" << endl;
  if (m_module->m_LM->saveUserBigramCacheAndCandidateOverrideCache()) {
    // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Cache written to disk"
    // << endl;
  }

  bench.stop();
  //    loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Elapsed time: " <<
  //    bench.elapsedSeconds() << endl;
}

void OVIMSmartMandarinContext::clear(OVLoaderService* loaderService) {
  // OV's default implementation of clear is startSession + stopSession, which
  // in our case will be very slow, so the cut.

  m_BPMFReading.setKeyboardLayout(currentKeyboardLayout());
  m_BPMFReading.clear();
  m_manjusri.clear();
  m_cursor = m_manjusri.cursorLeftBound();
  m_markMode = false;
  m_markCursor = m_cursor;
}

bool OVIMSmartMandarinContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                         OVTextBuffer* composingText,
                                         OVCandidateService* candidateService,
                                         OVLoaderService* loaderService) {
  OVIMSmartMandarinStringFilter filter(
      m_module->m_cfgUseCharactersSupportedByEncoding,
      loaderService->encodingService());

  if (0) {
    loaderService->logger("OVIMSmartMandarinContext")
        << "key code: " << key->keyCode()
        << (key->isCtrlPressed() ? ", ctrl" : "")
        << (key->isAltPressed() ? ", alt" : "")
        << (key->isOptPressed() ? ", opt" : "")
        << (key->isCommandPressed() ? ", cmd" : "")
        << (key->isCombinedFunctionKey() ? ", combied" : "") << endl;
  }

  // if it's non-printable key and the composing text is empty, we return the
  // event
  if (!key->receivedString().size() && readingText->isEmpty() &&
      composingText->isEmpty()) {
    return false;
  }

  // flush cache key
  if ((key->receivedString() == "2") && key->isCtrlPressed() &&
      readingText->isEmpty() && composingText->isEmpty()) {
    m_module->m_cacheFlushKeyCounter++;

    if (m_module->m_cacheFlushKeyCounter > 10) {
      m_module->m_cacheFlushKeyCounter = 0;
      m_module->m_LM->flushCache();
      m_module->m_LM->flushUserCache();
      m_module->m_LM->saveUserBigramCacheAndCandidateOverrideCache(true, true);

      composingText->showToolTip("All caches flushed.");
    }
    return false;
  } else {
    m_module->m_cacheFlushKeyCounter = 0;
  }

  bool capsLockFilter = true;

#if defined(__APPLE__)
  capsLockFilter = !key->isCapsLockOn();
#endif

  bool ctrlOptX =
      capsLockFilter &&
      (key->isCtrlPressed() && (key->isOptPressed() || key->isAltPressed()) &&
       key->isPrintable());
  bool ctrl0or1 =
      capsLockFilter &&
      ((key->receivedString() == "0" || key->receivedString() == "1") &&
       key->isCtrlPressed());
  bool ctrlX = capsLockFilter &&
               (key->isCtrlPressed() && key->isPrintable() && !ctrl0or1);
  bool punctuationListLongJump = false;

  // if (ctrlOptX || ctrl0or1) {
  //  loaderService->logger("SmartMandarin") << "Typed CTRL-OPT-x or
  //  CTRL-0/CTRL-1" << endl;
  // }

  // filter out combined function keys
  if (key->isCombinedFunctionKey() && !(ctrlOptX || ctrl0or1 || ctrlX)) {
    if (readingText->isEmpty() && composingText->isEmpty())
      return false;
    else {
      loaderService->beep();
      return true;
    }
  }

  if ((!key->keyCode() || key->isDirectTextKey()) &&
      key->receivedString().size()) {
    if (readingText->isEmpty() && composingText->isEmpty()) {
      composingText->setText(key->receivedString());
      composingText->commitAsTextSegment();
      return true;
    } else {
      m_BPMFReading.clear();
      readingText->clear();
      readingText->updateDisplay();
    }
  }

  unsigned int keyCode = 0;
  bool translatedCandidateKey = false;

  bool readingOnly = false;
  bool shouldCompose = false;
  bool shouldCommit = false;
  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "cursor before: " <<
  // m_cursor << endl;

  if (m_BPMFReading.isValidKey(key->keyCode()) && !key->isCapsLockOn() &&
      !key->isNumLockOn() && !key->isDirectTextKey() &&
      !(ctrlOptX || ctrl0or1 || ctrlX)) {
    m_BPMFReading.combineKey(key->keyCode());
    if (m_BPMFReading.hasToneMarker())
      shouldCompose = true;
    else
      readingOnly = true;
  }

  // @pinyin
  // if (key->keyCode() >= 'a' && key->keyCode() <= 'z') {
  //     shouldCompose = true;
  // }
  // @pinyin
  else {
    // mark mode processing
    if (m_BPMFReading.isEmpty() && key->isShiftPressed() &&
        (key->keyCode() == OVKeyCode::Left ||
         key->keyCode() == OVKeyCode::Right ||
         key->keyCode() == OVKeyCode::Home ||
         key->keyCode() == OVKeyCode::End)) {
      // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "SHIFT MODE" << endl;

      if (!m_markMode) {
        m_markMode = true;
        m_markCursor = m_cursor;
      }

      if (key->keyCode() == OVKeyCode::Right)
        m_cursor++;
      else if (key->keyCode() == OVKeyCode::Left)
        m_cursor--;
      else if (key->keyCode() == OVKeyCode::Home)
        m_cursor = m_manjusri.cursorLeftBound();
      else if (key->keyCode() == OVKeyCode::End)
        m_cursor = m_manjusri.cursorRightBound();

      if (m_markCursor > m_cursor && m_markCursor - m_cursor > 6)
        m_cursor = m_markCursor - 6;
      if (m_markCursor < m_cursor && m_markCursor - m_cursor < -6)
        m_cursor = m_markCursor + 6;

      if (m_cursor < m_manjusri.cursorLeftBound())
        m_cursor = m_manjusri.cursorLeftBound();
      if (m_cursor > m_manjusri.cursorRightBound())
        m_cursor = m_manjusri.cursorRightBound();

      size_t from = m_markCursor;
      size_t to = m_cursor;

      if (from > to) swap(from, to);

      composingText->clear();
      composingText->setText(m_manjusri.composedString());
      composingText->setCursorPosition(m_cursor - m_manjusri.cursorLeftBound());

      if (!(to - from)) {
        m_markMode = false;
        loaderService->beep();
      } else {
        string current = m_manjusri.currentlyMarkedUnigram(from, to);

        from -= m_manjusri.cursorLeftBound();
        to -= m_manjusri.cursorLeftBound();
        composingText->setHighlightMark(
            OVTextBuffer::RangePair(from, to - from));

        if (loaderService->locale() == "zh_TW" ||
            loaderService->locale() == "zh-Hant")
          composingText->showToolTip(
              string("\xe6\xad\xa3\xe5\x9c\xa8\xe9\x81\xb8\xe5\x8f\x96\xe5\xad"
                     "\x97\xe8\xa9\x9e\xe7\xb5\x84\xef\xbc\x9a") +
              current + string("\xef\xbc\x8c\xe8\xab\x8b\xe6\x8c\x89") +
              string("ENTER\xe9\x8d\xb5\xe5\x8a\xa0\xe5\x85\xa5\xe8\xb3\x87\xe6"
                     "\x96\x99\xe5\xba\xab"));
        else if (loaderService->locale() == "zh_CN" ||
                 loaderService->locale() == "zh-Hans")
          composingText->showToolTip(
              string("\xe6\xad\xa3\xe5\x9c\xa8\xe9\x80\x89\xe5\x8f\x96\xe5\xad"
                     "\x97\xe8\xaf\x8d\xe7\xbb\x84\xef\xbc\x9a") +
              current + string("\xef\xbc\x8c\xe8\xaf\xb7\xE6\x8C\x89") +
              string("ENTER\xe9\x94\xae\xe5\x8a\xa0\xe5\x85\xa5\xe6\x95\xb0\xe6"
                     "\x8d\xae\xe5\xba\x93"));
        else
          composingText->showToolTip(
              string("Currently marked: \"") + current +
              string("\", press ENTER to add into the database."));
      }
      composingText->updateDisplay();
      return true;
    } else {
      if (m_markMode && key->keyCode() == OVKeyCode::Return) {
        m_markMode = false;

        size_t from = m_markCursor;
        size_t to = m_cursor;

        if (from > to) swap(from, to);

        // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Adding phrase at "
        // << from << "-" << to << ": " <<
        // m_manjusri.composedString().substr(from -
        // m_manjusri.cursorLeftBound(), to - from) << endl;
        pair<bool, string> result = m_manjusri.addUserUnigram(from, to);
        string& current = result.second;

        composingText->clear();

        if (result.first) {
          if (loaderService->locale() == "zh_TW" ||
              loaderService->locale() == "zh-Hant")
            composingText->showToolTip(
                string("\xE5\x8A\xA0\xE5\x85\xA5\xE6\x96\xB0\xE8\xA9\x9E\xEF"
                       "\xBC\x9A\xE3\x80\x8C") +
                current + string("\xE3\x80\x8D"));
          else if (loaderService->locale() == "zh_CN" ||
                   loaderService->locale() == "zh-Hans")
            composingText->showToolTip(
                string("\xE5\x8A\xA0\xE5\x85\xA5\xE6\x96\xB0\xE8\xAF\x8D\xEF"
                       "\xBC\x9A\xE2\x80\x9C") +
                current + string("\xE2\x80\x9D"));
          else
            composingText->showToolTip(string("New phrase \"") + current +
                                       string("\" added."));
        } else {
          if (loaderService->locale() == "zh_TW" ||
              loaderService->locale() == "zh-Hant")
            composingText->showToolTip(
                string("\xE3\x80\x8C") + current +
                string("\xe3\x80\x8d\xe7\x84\xa1\xe6\xb3\x95\xe5\x8a\xa0\xe5"
                       "\x85\xa5\xef\xbc\x8c\xe8\xa9\xb2\xe8\xa9\x9e\xe5\xb7"
                       "\xb2\xe7\xb6\x93\xe5\xad\x98\xe5\x9c\xa8\xe6\x96\xbc"
                       "\xe8\xb3\x87\xe6\x96\x99\xe5\xba\xab\xe4\xb8\xad"));
          else if (loaderService->locale() == "zh_CN" ||
                   loaderService->locale() == "zh-Hans")
            composingText->showToolTip(
                string("\xE2\x80\x9C") + current +
                string("\xE2\x80\x9D\xe6\x97\xa0\xe6\xb3\x95\xe5\x8a\xa0\xe5"
                       "\x85\xa5\xef\xbc\x8c\xe8\xaf\xa5\xe8\xa9\x9e\xe5\xb7"
                       "\xb2\xe7\xb6\x93\xe5\xad\x98\xe5\x9c\xa8\xe4\xba\x8e"
                       "\xe6\x95\xb0\xe6\x8d\xae\xe5\xba\x93\xe4\xb8\xad"));
          else
            composingText->showToolTip(
                string("Cannot add \"") + current +
                string("\". It may already exist in the database."));
        }

        composingText->setText(m_manjusri.composedString());
        composingText->setCursorPosition(m_cursor -
                                         m_manjusri.cursorLeftBound());
        composingText->updateDisplay();
        return true;
      }

      if (m_markMode && key->keyCode() == OVKeyCode::Esc ||
          m_markMode && key->keyCode() == OVKeyCode::Left ||
          m_markMode && key->keyCode() == OVKeyCode::Right) {
        m_markMode = false;
        composingText->clear();
        composingText->setText(m_manjusri.composedString());
        composingText->setCursorPosition(m_cursor -
                                         m_manjusri.cursorLeftBound());
        composingText->updateDisplay();
        return true;
      }

      if (m_markMode) {
        loaderService->beep();
        m_cursor = m_markCursor;
        m_markMode = false;
      }
    }

    keyCode = key->keyCode();

    if (keyCode == OVKeyCode::Space) {
      if (m_module->m_cfgShowCandidateListWithSpace &&
          m_BPMFReading.isEmpty() && !key->isCapsLockOn()) {
        keyCode = OVKeyCode::Down;
      } else {
        if (!m_BPMFReading.isEmpty()) {
          shouldCompose = true;
        }
      }
    } else if (keyCode == 'q') {
      // translates q key if in Hsu layout
      const BopomofoKeyboardLayout* layout =
          BopomofoKeyboardLayout::LayoutForName(m_module->m_cfgKeyboardLayout);
      if (layout == BopomofoKeyboardLayout::HsuLayout()) {
        if (!key->isCapsLockOn() && !key->isShiftPressed() &&
            !key->isDirectTextKey() && !composingText->isEmpty()) {
          keyCode = OVKeyCode::Down;
          translatedCandidateKey = true;
        }
      }
    }

    switch (keyCode) {
      case OVKeyCode::Home:
      case OVKeyCode::End:
        if (!m_BPMFReading.isEmpty()) {
          loaderService->beep();
        } else {
          if (composingText->isEmpty()) return false;

          if (key->keyCode() == OVKeyCode::Home) {
            if (m_cursor > m_manjusri.cursorLeftBound())
              m_cursor = m_manjusri.cursorLeftBound();
            else
              loaderService->beep();
          } else {
            if (m_cursor < m_manjusri.cursorRightBound())
              m_cursor = m_manjusri.cursorRightBound();
            else
              loaderService->beep();
          }
        }
        break;
      case OVKeyCode::Left:
      case OVKeyCode::Right:
        if (!m_BPMFReading.isEmpty()) {
          loaderService->beep();
        } else {
          if (composingText->isEmpty()) return false;

          if (key->keyCode() == OVKeyCode::Left) {
            if (m_cursor > m_manjusri.cursorLeftBound())
              m_cursor--;
            else
              loaderService->beep();
          } else {
            if (m_cursor < m_manjusri.cursorRightBound())
              m_cursor++;
            else
              loaderService->beep();
          }
        }
        break;

      // case OVKeyCode::Space:
      case OVKeyCode::Return:
      case OVKeyCode::Down:
      CallForthCandidateWindow:
        if (!m_BPMFReading.isEmpty()) {
          shouldCompose = true;
        } else if (!composingText->isEmpty()) {
          if (key->keyCode() == OVKeyCode::Return) {
            shouldCommit = true;
          } else if (key->keyCode() == OVKeyCode::Space ||
                     key->keyCode() == OVKeyCode::Down ||
                     punctuationListLongJump || translatedCandidateKey) {
            // we use the original key code to determine the key

            size_t candidateCursor = m_cursor;
            if (!m_module->m_cfgCandidateCursorAtEndOfTargetBlock &&
                candidateCursor == m_manjusri.cursorRightBound())
              candidateCursor--;

            if (m_module->m_cfgCandidateCursorAtEndOfTargetBlock &&
                candidateCursor == m_manjusri.cursorLeftBound())
              candidateCursor++;

            vector<string> candidates = m_manjusri.collectCandidates(
                candidateCursor,
                m_module->m_cfgCandidateCursorAtEndOfTargetBlock);

            if (candidates.size() < 2) {
              loaderService->beep();
            } else {
              OVOneDimensionalCandidatePanel* panel =
                  candidateService->useOneDimensionalCandidatePanel();
              OVCandidateList* candidateList = panel->candidateList();
              candidateList->setCandidates(candidates);
              OVKeyVector nextPageKeys = panel->defaultNextPageKeys();
              OVKeyVector previousPageKeys = panel->defaultPreviousPageKeys();
              nextPageKeys.push_back(
                  loaderService->makeOVKey('>', false, false, false, true));
              previousPageKeys.push_back(
                  loaderService->makeOVKey('<', false, false, false, true));
              panel->setNextPageKeys(nextPageKeys);
              panel->setPreviousPageKeys(previousPageKeys);

              if (m_module->m_cfgCandidateSelectionKeys.size()) {
                panel->setCandidateKeys(m_module->m_cfgCandidateSelectionKeys,
                                        loaderService);
                panel->setCandidatesPerPage(
                    m_module->m_cfgCandidateSelectionKeys.size());
              } else {
                const BopomofoKeyboardLayout* layout =
                    BopomofoKeyboardLayout::LayoutForName(
                        m_module->m_cfgKeyboardLayout);

                if (layout == BopomofoKeyboardLayout::HsuLayout()) {
                  panel->setCandidateKeys("asdfzxcv", loaderService);
                  panel->setCandidatesPerPage(8);
                } else if (layout == BopomofoKeyboardLayout::ETen26Layout()) {
                  panel->setCandidateKeys("asdfjkl;", loaderService);
                  panel->setCandidatesPerPage(8);
                } else {
                  panel->setCandidateKeys("12345678", loaderService);
                  panel->setCandidatesPerPage(8);
                }
              }

              panel->updateDisplay();
              panel->show();
              panel->yieldToCandidateEventHandler();
              return true;
            }
          } else {
            loaderService->beep();
            return true;
          }
        } else {
          return false;
        }
        break;

      case OVKeyCode::Esc:
        if (!m_BPMFReading.isEmpty()) {
          m_BPMFReading.clear();
        } else {
          if (!composingText->isEmpty()) {
            if (!m_module->m_cfgClearComposingTextWithEsc) {
              loaderService->beep();
              return true;
            } else {
              m_manjusri.clear();
            }
          } else {
            return false;
          }
        }
        break;

      case OVKeyCode::Backspace:
        if (!m_BPMFReading.isEmpty()) {
          m_BPMFReading.backspace();
        } else {
          if (!composingText->isEmpty()) {
            if (m_cursor > m_manjusri.cursorLeftBound()) {
              m_manjusri.backspaceAt(m_cursor, &filter);
              m_cursor--;
            } else {
              loaderService->beep();
            }
          } else {
            return false;
          }
        }
        break;

      case OVKeyCode::Delete:
        if (!m_BPMFReading.isEmpty()) {
          loaderService->beep();
          return true;
        } else {
          if (!composingText->isEmpty()) {
            if (m_cursor < m_manjusri.cursorRightBound()) {
              m_manjusri.deleteAt(m_cursor, &filter);
            } else {
              loaderService->beep();
            }
          } else {
            return false;
          }
        }
        break;

        /*
                        case 'q':
                                {
                            const BopomofoKeyboardLayout* layout =
           BopomofoKeyboardLayout::LayoutForName(m_module->m_cfgKeyboardLayout);
                            if (layout == BopomofoKeyboardLayout::HsuLayout())
                                        {
                                                if (!key->isCapsLockOn() &&
           !key->isShiftPressed() && !key->isDirectTextKey() &&
           (!composingText->isEmpty() || !readingText->isEmpty())) { return
           true;
                                                }
                                        }
                                }
        */
      default:
        if (!m_BPMFReading.isEmpty()) {
          // ignores space (shouldCompose == true)
          if (!shouldCompose) {
            loaderService->beep();
          }
        } else {
          if (ctrlOptX || ctrl0or1 || ctrlX) {
            // handles ctrl-opt-[x] or ctrl-alt-[x]
            string queryString;
            //					loaderService->logger("debug") << "blah"
            //<< endl;

            string keyString = key->receivedString();
            if (keyString.length() > 1) {
              // if received, say full-width character
              unsigned int kc = key->keyCode();
              if (kc && kc < 128) {
                if (kc >= 'A' && kc <= 'Z') {
                  keyString = string(1, kc - 'A' + 'a');
                } else {
                  keyString = string(1, kc);
                }
              }
            }

            if (ctrlOptX) {
              queryString = string("_ctrl_opt_") + keyString;
            } else if (ctrlX) {
              queryString = string("_ctrl_") + keyString;
            } else {
              queryString = string("_punctuation_list");
            }

            if (!queryString.size()) {
              loaderService->beep();
            } else if (!m_module->m_LM->isInDictionary(queryString, true,
                                                       &filter)) {
              //						loaderService->logger("debug")
              //<< "didn't find!" << endl;

#if defined(__APPLE__)
              if (readingText->isEmpty() && composingText->isEmpty()) {
                // don't eat up ctrl keys if not a punctuation key
                return false;
              } else {
                loaderService->beep();
              }
#else
              loaderService->beep();
#endif
            } else {
              if (m_manjusri.insertAt(m_cursor, queryString, &filter)) {
                m_cursor++;
                m_manjusri.update();

                UnigramVector univec =
                    m_module->m_LM->findUnigrams(queryString, true, &filter);

                // see if we need to call forth composition window for now
                if (univec.size() > 1) {
                  punctuationListLongJump = true;
                }
              }
            }
          } else {
            bool composingBufferEmpty = composingText->isEmpty();

            string prefix = "_punctuation_";
            string punctuation;
            bool found = false;
            bool byPassPunctuation =
                (key->isCapsLockOn() || key->isDirectTextKey() ||
                 !key->keyCode());

            if (!byPassPunctuation) {
              vector<string> punctuationQueries =
                  OVIMMandarinPunctuationQueriesForKey(currentKeyboardLayout(),
                                                       key);
              for (vector<string>::const_iterator iter =
                       punctuationQueries.begin();
                   iter != punctuationQueries.end(); ++iter) {
                if (!m_module->m_LM->isInDictionary(*iter, true, &filter))
                  continue;

                punctuation = *iter;
                found = true;
                break;
              }
            }

            string keystr = key->receivedString();
            if (!found && keystr.size()) {

              if (keystr.size() == 1 &&
                  (key->isCapsLockOn() || key->isShiftPressed())) {
                // reverse the case that is sent in (OS X only)
                char c = key->keyCode();

                if (c >= 'A' && c <= 'Z')
                  c = tolower(c);
                else
                  c = toupper(c);

                // OS X doesn't send lower case letters when CAPS LOCK + SHIFT
                // are both on, so here
                if (key->isCapsLockOn() && key->isShiftPressed())
                  c = toupper(c);

                if (m_module->m_cfgShiftKeyAlwaysCommitUppercaseCharacters &&
                    key->isShiftPressed()) {
                  c = toupper(c);
                }

                keystr = string(1, c);
              }

              prefix = "_passthru_";

              if (keystr == " ") keystr = "space";

              punctuation = string("_passthru_") + keystr + " ";
              found = true;
            }

            if (!found) {
              loaderService->beep();
            } else {
              if (m_manjusri.insertAt(m_cursor, punctuation, &filter)) {
                m_cursor++;

                UnigramVector univec =
                    m_module->m_LM->findUnigrams(punctuation, true, &filter);
                if (composingBufferEmpty && univec.size() == 1) {
                  m_manjusri.update();
                  m_manjusri.logStats(loaderService);

                  if (prefix != "_punctuation_") shouldCommit = true;
                }
              } else {
                loaderService->beep();
              }
            }
          }
        }
    }  // switch
  }

  if (shouldCommit) {
    vector<string> textSegments = m_manjusri.composedStringAsTextSegments();
    for (vector<string>::const_iterator tsiter = textSegments.begin();
         tsiter != textSegments.end(); ++tsiter) {
      composingText->setText(*tsiter);
      composingText->commitAsTextSegment();
    }

    m_manjusri.clear();
    m_cursor = m_manjusri.cursorLeftBound();
    m_markMode = false;
    m_markCursor = m_cursor;
    return true;
  }

  if (shouldCompose) {
    if (m_manjusri.insertAt(m_cursor,
                            m_BPMFReading.syllable().absoluteOrderString(),
                            &filter)) {
      m_cursor++;
      m_BPMFReading.clear();
    }
    // @pinyin
    // if (m_manjusri.insertAt(m_cursor, key->receivedString())) {
    //     m_cursor++;
    // }
    // @pinyin
    else
      loaderService->beep();
  }

  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "cursor after: " <<
  // m_cursor << endl;

  if (!readingOnly) {
    m_manjusri.update();
    m_manjusri.logStats(loaderService);

    if (m_cursor < m_manjusri.cursorLeftBound())
      m_cursor = m_manjusri.cursorLeftBound();

    if (m_cursor > m_manjusri.cursorRightBound())
      m_cursor = m_manjusri.cursorRightBound();
  }

  // comment out the two lines below for Pinyin
  readingText->setText(m_BPMFReading.composedString());
  readingText->updateDisplay();

  composingText->clear();

  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Cursor bound: " <<
  // m_manjusri.cursorRightBound() << endl;
  if (m_BPMFReading.isEmpty() &&
      m_manjusri.cursorRightBound() >
          (m_module->m_cfgComposingTextBufferSize + 1) /* 31 */) {
    // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "popping" << endl;

    string popped;
    string nextComposed;
    bool continuePop;

    // we need to pop up the half-width spaces that come after the popped-up
    // chars, otherwise some Windows app (e.g. Outlook Express) will act with
    // quirks (e.g. repeated commit of present composing buffer)
    do {
      continuePop = false;
      string newlyPopped = m_manjusri.shift();
      popped += newlyPopped;
      if (m_cursor != m_manjusri.cursorLeftBound())
        m_cursor -= OVUTF8Helper::SplitStringByCodePoint(newlyPopped).size();
      m_manjusri.update();
      m_manjusri.logStats(loaderService);

      nextComposed = m_manjusri.composedString();
      if (nextComposed.size()) {
        if (nextComposed[0] == ' ') {
          continuePop = true;
        }
      }
    } while (continuePop);

    composingText->setText(popped);
    composingText->commitAsTextSegment();
  }

  vector<pair<size_t, size_t> > wordsegs = m_manjusri.wordSegments();
  // for (vector<pair<size_t, size_t> >::iterator vpiter = wordsegs.begin() ;
  // vpiter != wordsegs.end() ; ++vpiter) {
  //     cerr << "word seg: (" << (*vpiter).first << ", " << (*vpiter).second <<
  //     ")" << endl;
  // }

  composingText->setText(m_manjusri.composedString());
  composingText->setCursorPosition(m_cursor - m_manjusri.cursorLeftBound());
  composingText->setWordSegments(wordsegs);
  composingText->updateDisplay();

  if (punctuationListLongJump) {
    goto CallForthCandidateWindow;
  }

  return true;
}

void OVIMSmartMandarinContext::candidateCanceled(
    OVCandidateService* candidateService, OVTextBuffer* readingText,
    OVTextBuffer* composingText, OVLoaderService* loaderService) {}

bool OVIMSmartMandarinContext::candidateSelected(
    OVCandidateService* candidateService, const string& text, size_t index,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  size_t newCursorPosition = m_manjusri.chooseCandidate(index);
  composingText->setText(m_manjusri.composedString());

  // if it's New Phonetic style, we move the cursor
  if (!m_module->m_cfgCandidateCursorAtEndOfTargetBlock) {
    m_cursor = newCursorPosition;
  }

  if (m_cursor < m_manjusri.cursorLeftBound())
    m_cursor = m_manjusri.cursorLeftBound();

  if (m_cursor > m_manjusri.cursorRightBound())
    m_cursor = m_manjusri.cursorRightBound();

  composingText->setCursorPosition(m_cursor - m_manjusri.cursorLeftBound());
  vector<pair<size_t, size_t> > wordsegs = m_manjusri.wordSegments();
  composingText->setWordSegments(wordsegs);

  composingText->updateDisplay();
  return true;
}

bool OVIMSmartMandarinContext::candidateNonPanelKeyReceived(
    OVCandidateService* candidateService, const OVKey* key,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  return false;
}

OVIMSmartMandarin::OVIMSmartMandarin()
    : m_LM(0),
      m_cacheFlushKeyCounter(0)
      //    , m_BPMFDB(0)
      ,
      m_cfgCandidateCursorAtEndOfTargetBlock(false),
      m_cfgComposingTextBufferSize(10)
#ifndef WIN32
      ,
      m_cfgShowCandidateListWithSpace(true),
      m_cfgClearComposingTextWithEsc(false)
#else
      ,
      m_cfgShowCandidateListWithSpace(false),
      m_cfgClearComposingTextWithEsc(true)
#endif
      ,
      m_cfgShiftKeyAlwaysCommitUppercaseCharacters(false) {
}

OVIMSmartMandarin::~OVIMSmartMandarin() {
  if (m_LM) delete m_LM;

  // if (m_BPMFDB)
  //     delete m_BPMFDB;
}

OVEventHandlingContext* OVIMSmartMandarin::createContext() {
  return new OVIMSmartMandarinContext(this);
}

const string OVIMSmartMandarin::identifier() const {
  return string(OVIMSMARTMANDARIN_IDENTIFIER);
}

const string OVIMSmartMandarin::localizedName(const string& locale) {
#ifndef _MSC_VER
  string tcname = "智慧注音";
  string scname = "智慧注音";
#else
  string tcname = "\xE6\x99\xBA\xE6\x85\xA7\xE6\xB3\xA8\xE9\x9F\xB3";
  string scname = "\xE6\x99\xBA\xE6\x85\xA7\xE6\xB3\xA8\xE9\x9F\xB3";
#endif

  if (locale == "zh_TW" || locale == "zh-Hant")
    return tcname;
  else if (locale == "zh_CN" || locale == "zh-Hans")
    return scname;
  else
    return string("Smart Mandarin");
}

bool OVIMSmartMandarin::initialize(OVPathInfo* pathInfo,
                                   OVLoaderService* loaderService) {
  OVSQLiteDatabaseService* dbService = dynamic_cast<OVSQLiteDatabaseService*>(
      loaderService->SQLiteDatabaseService());
  if (!dbService) {
    loaderService->logger(OVIMMANDARIN_IDENTIFIER)
        << "cannot obtain SQLite-backed DB service" << endl;
    return false;
  }

  // string dbPath = OVPathHelper::PathCat(pathInfo->resourcePath,
  // "OpenVanilla.db"); string dbPath = dbService->filename();
  //
  //     if (!OVPathHelper::PathExists(dbPath)) {
  //         loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "db not found: "
  //         << dbPath << endl; return false;
  //     }
  // else {
  //  loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "attempting to open: "
  //  << dbPath << endl;
  // }

  OVSQLiteConnection* lmdb = dbService->connection();

  // lmdb = OVSQLiteConnection::Open(dbPath);
  // if (!lmdb) {
  //     loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "cannot open: " <<
  //     dbPath << endl; return false;
  // }

  if (!lmdb->hasTable("bigrams") || !lmdb->hasTable("unigrams")) {
    loaderService->logger(OVIMMANDARIN_IDENTIFIER)
        << "doesn't have table required." << endl;  // : " << dbPath << endl;
    // delete lmdb;
    return false;
  }

  //    loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "using: " << dbPath <<
  //    endl;

  OVKeyValueDataTableInterface* externalBPMFTable = 0;

  externalBPMFTable =
      loaderService->SQLiteDatabaseService()->createKeyValueDataTableInterface(
          "Mandarin-bpmf-cin");

  // dbPath = OVPathHelper::PathCat(pathInfo->resourcePath, "BPMF.db");
  // m_BPMFDB = OVSQLiteConnection::Open(dbPath);
  //
  // if (m_BPMFDB) {
  //     if (m_BPMFDB->hasTable("Manjusri-bpmf-cin")) {
  //         OVSQLiteDatabaseService* dbService =
  //         OVSQLiteDatabaseService::ServiceWithExistingConnection(m_BPMFDB);
  //         externalBPMFTable =
  //         dbService->createKeyValueDataTableInterface("Manjusri-bpmf-cin");
  //         if (externalBPMFTable)
  //             loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "using: " <<
  //             dbPath << endl;
  //     }
  // }

  if (!externalBPMFTable)
    loaderService->logger(OVIMMANDARIN_IDENTIFIER)
        << "no external BPMF table is used" << endl;

  bool useUserTable = false;

  OVDirectoryHelper::CheckDirectory(pathInfo->writablePath);

  string dbPath;
  dbPath =
      OVPathHelper::PathCat(pathInfo->writablePath, "SmartMandarinUserData.db");
  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "attempting to open " <<
  // dbPath << endl;

  OVSQLiteConnection* userDB = OVSQLiteConnection::Open(dbPath);

  // attempt to open the DB clear, if it's readable, we must rekey it
  //     if (userDB) {
  //         OVSQLiteStatement* tryStatement = userDB->prepare("SELECT * FROM
  //         sqlite_master"); if (tryStatement) {
  //             while (tryStatement->step() == SQLITE_ROW) {}
  //             loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "DB is in
  //             clear text, we must rekey" << endl;
  //
  //             char* key = UserDonnecle();
  //             int result = sqlite3_rekey(userDB->connection(), key,
  //             strlen(key)); free(key);
  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Rekey returns: " <<
  // result << endl;
  //         }
  //         else {
  //             loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "DB is
  //             probably not in clear text" << endl;
  //         }
  //     }

  if (userDB) {
    //        pair<char*, size_t> cle = ObtenirUserDonneCle();
    //        if (cle.first) {
    //            sqlite3_key(userDB->connection(), cle.first, (int)cle.second);
    //            free(cle.first);
    //        }

    OVSQLiteStatement* tryStatement;

    tryStatement = userDB->prepare("SELECT * FROM sqlite_master");
    if (tryStatement) {
      // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "DB opened with the
      // new key" << endl;
      while (tryStatement->step() == SQLITE_ROW) {
      }
      delete tryStatement;
    } else {
      delete userDB;
      userDB = OVSQLiteConnection::Open(dbPath);
      // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "DB is not openable,
      // retrying with the old key" << endl;

      //            pair<char*, size_t> cle = ObtenirUserDonneCle();
      //            if (cle.first) {
      //                sqlite3_key(userDB->connection(), cle.first,
      //                (int)cle.second); free(cle.first);
      //            }

      tryStatement = userDB->prepare("SELECT * FROM sqlite_master");
      if (!tryStatement) {
        delete userDB;
        userDB = 0;
        loaderService->logger(OVIMMANDARIN_IDENTIFIER)
            << "DB is not openable" << endl;
      } else {
        // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "DB opened with the
        // old key, rekeying" << endl;
        //                while (tryStatement->step() == SQLITE_ROW) {}
        //                sqlite3_rekey(userDB->connection(), "", 0);
        //                delete tryStatement;
      }
    }
  }

  if (userDB) {
    if (!userDB->hasTable("user_unigrams")) {
      userDB->createTable("user_unigrams",
                          "qstring, current, probability, backoff");
      userDB->createIndexOnTable("user_unigrams_index", "user_unigrams",
                                 "qstring");
    }

    if (!userDB->hasTable("user_bigram_cache")) {
      userDB->createTable("user_bigram_cache",
                          "qstring, previous, current, probability");
      userDB->createIndexOnTable("user_bigram_cache_index", "user_bigram_cache",
                                 "qstring");
    }

    if (!userDB->hasTable("user_candidate_override_cache")) {
      userDB->createTable("user_candidate_override_cache", "qstring, current");
      userDB->createIndexOnTable("user_candidate_override_cache_index",
                                 "user_candidate_override_cache", "qstring");
    }

    // import and remove the old database
    string oldDBPath;
    oldDBPath = OVPathHelper::PathCat(pathInfo->writablePath, "UserPhrase.db");
    //        loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "attempting to
    //        open old database: " << dbPath << endl;
    OVSQLiteConnection* oldUserDB;
    bool done = false;

    if (OVPathHelper::PathExists(oldDBPath)) {
      oldUserDB = OVSQLiteConnection::Open(oldDBPath);
    } else {
      //			loaderService->logger(OVIMMANDARIN_IDENTIFIER)
      //<< "Doesn't exist" << endl;
      oldUserDB = 0;
    }

    if (oldUserDB) {
      // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "old db opened,
      // importing" << endl;

      if (userDB->execute("BEGIN") == SQLITE_OK) {
        OVSQLiteStatement* fetch =
            oldUserDB->prepare("SELECT * FROM user_unigrams");

        if (fetch) {
          while (fetch->step() == SQLITE_ROW) {
            // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "qstring " <<
            // fetch->textOfColumn(0) << endl;

            // #warning do we need to free textOfColumn()'s return value?
            // look-it up at SQLite doc
            userDB->execute("INSERT INTO user_unigrams VALUES(%Q, %Q, %Q, %f)",
                            fetch->textOfColumn(0), fetch->textOfColumn(1),
                            fetch->textOfColumn(2), fetch->doubleOfColumn(3));
          }
        } else {
          // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "prepare failed"
          // << endl;
        }

        if (userDB->execute("COMMIT") == SQLITE_OK) done = true;
      }

      if (done) {
        delete oldUserDB;
        OVPathHelper::RemoveEverythingAtPath(oldDBPath);
      } else {
        delete oldUserDB;
      }
    }

    if (userDB->execute("PRAGMA synchronous = OFF") == SQLITE_OK) {
      // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "pragma executed" <<
      // endl;
    } else {
      // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "pragma execution
      // failed" << endl;
    }

    // close userDB, now we'll attach the new stuff
    delete userDB;

    //        #ifndef OVIMSMARTMANDARIN_USE_SQLITE_CRYPTO
    if (lmdb->execute("ATTACH DATABASE %Q AS userdb", dbPath.c_str()) ==
        SQLITE_OK)
      useUserTable = true;
    //        #else
    //            pair<char*, size_t> cle = ObtenirUserDonneCle();
    //            if (cle.first) {
    //                char* key = (char*)calloc(1, cle.second + 1);
    //                memcpy(key, cle.first, cle.second);
    //                if (lmdb->execute("ATTACH DATABASE %Q AS userdb KEY %Q",
    //                dbPath.c_str(), key) == SQLITE_OK)
    //                    useUserTable = true;
    //                free(key);
    //                free(cle.first);
    //            }
    //        #endif
  }

  // if (useUserTable)
  //     loaderService->logger(OVIMMANDARIN_IDENTIFIER) <<  "using user table: "
  //     << dbPath << endl;
  // else
  //     loaderService->logger(OVIMMANDARIN_IDENTIFIER) <<  "no user table is
  //     being used" << endl;

  // important! set ownsDBConnection to false!
  m_LM = new LanguageModel(lmdb, externalBPMFTable, useUserTable, false, false,
                           useUserTable, useUserTable);

  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "loading user bigram
  // cache" << endl;
  m_LM->loadUserBigramCache();

  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "loading user candidate
  // override cache" << endl;
  m_LM->loadUserCandidateOverrideCache();

  Node::SetUNK(m_LM->UNKUnigram().probability, m_LM->UNKUnigram().backoff);

  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << OVIMMANDARIN_IDENTIFIER
  // << " initialized." << endl;
  return true;
}

void OVIMSmartMandarin::finalize() {}

void OVIMSmartMandarin::loadConfig(OVKeyValueMap* moduleConfig,
                                   OVLoaderService* loaderService) {
  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "loading config" << endl;

  if (moduleConfig->hasKey("KeyboardLayout"))
    m_cfgKeyboardLayout = moduleConfig->stringValueForKey("KeyboardLayout");
  else
    m_cfgKeyboardLayout = "Standard";

  if (moduleConfig->hasKey("CandidateSelectionKeys"))
    m_cfgCandidateSelectionKeys =
        moduleConfig->stringValueForKey("CandidateSelectionKeys");
  else
    m_cfgCandidateSelectionKeys = "";

  if (moduleConfig->hasKey("CandidateCursorAtEndOfTargetBlock"))
    m_cfgCandidateCursorAtEndOfTargetBlock =
        moduleConfig->isKeyTrue("CandidateCursorAtEndOfTargetBlock");

  if (moduleConfig->hasKey("UseCharactersSupportedByEncoding")) {
    string encoding =
        moduleConfig->stringValueForKey("UseCharactersSupportedByEncoding");

    // if (encoding != m_cfgUseCharactersSupportedByEncoding)
    //     m_LM->flushCache();

    m_cfgUseCharactersSupportedByEncoding = encoding;
  }

  if (moduleConfig->hasKey("ShowCandidateListWithSpace")) {
    m_cfgShowCandidateListWithSpace =
        moduleConfig->isKeyTrue("ShowCandidateListWithSpace");
  }

  if (moduleConfig->hasKey("ClearComposingTextWithEsc")) {
    m_cfgClearComposingTextWithEsc =
        moduleConfig->isKeyTrue("ClearComposingTextWithEsc");
  }

  if (moduleConfig->hasKey("ComposingTextBufferSize")) {
    size_t s = (size_t)moduleConfig->intValueForKey("ComposingTextBufferSize");

    if (s > 0) m_cfgComposingTextBufferSize = s;
  }

  if (moduleConfig->hasKey("ShiftKeyAlwaysCommitUppercaseCharacters")) {
    m_cfgShiftKeyAlwaysCommitUppercaseCharacters =
        moduleConfig->isKeyTrue("ShiftKeyAlwaysCommitUppercaseCharacters");
  } else {
    m_cfgShiftKeyAlwaysCommitUppercaseCharacters = false;
  }

  if (m_cfgUseCharactersSupportedByEncoding.length())
    if (!loaderService->encodingService()->isEncodingSupported(
            m_cfgUseCharactersSupportedByEncoding))
      m_cfgUseCharactersSupportedByEncoding = "";

  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Using keyboard layout: "
  // << m_cfgKeyboardLayout << endl;
  // loaderService->logger(OVIMMANDARIN_IDENTIFIER) << "Candidate cursor
  // precesdes block: " << m_cfgCandidateCursorAtEndOfTargetBlock << endl;

  m_LM->flushCache();
  m_LM->loadUserBigramCache();
  m_LM->loadUserCandidateOverrideCache();
}

void OVIMSmartMandarin::saveConfig(OVKeyValueMap* moduleConfig,
                                   OVLoaderService* loaderService) {
  moduleConfig->setKeyStringValue("KeyboardLayout", m_cfgKeyboardLayout);
  moduleConfig->setKeyBoolValue("CandidateCursorAtEndOfTargetBlock",
                                m_cfgCandidateCursorAtEndOfTargetBlock);
  moduleConfig->setKeyStringValue("UseCharactersSupportedByEncoding",
                                  m_cfgUseCharactersSupportedByEncoding);
  moduleConfig->setKeyBoolValue("ShowCandidateListWithSpace",
                                m_cfgShowCandidateListWithSpace);
  moduleConfig->setKeyBoolValue("ClearComposingTextWithEsc",
                                m_cfgClearComposingTextWithEsc);

  if (m_cfgCandidateSelectionKeys.size())
    moduleConfig->setKeyStringValue("CandidateSelectionKeys",
                                    m_cfgCandidateSelectionKeys);
}

};  // namespace OpenVanilla
