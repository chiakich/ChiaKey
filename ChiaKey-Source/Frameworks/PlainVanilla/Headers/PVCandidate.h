//
// PVCandidate.h
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

#ifndef PVCandidate_h
#define PVCandidate_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class PVCandidateList : public OVCandidateList {
 public:
  virtual void clear() { m_list.clear(); }

  virtual size_t size() const { return m_list.size(); }

  virtual string candidateAtIndex(size_t index) const {
    if (index >= m_list.size()) return string();

    return m_list[index];
  }

  virtual void setCandidate(size_t index, const string& candidate) {
    if (index >= m_list.size()) {
      for (size_t inc = m_list.size(); inc <= index; inc++)
        m_list.push_back(string());
    }

    m_list[index] = candidate;
  }

  virtual void setCandidates(const vector<string>& candidates) {
    m_list = candidates;
  }

  virtual void addCandidate(const string& candidate) {
    m_list.push_back(candidate);
  }

  virtual void addCandidates(const vector<string>& candidates) {
    for (size_t index = 0; index < candidates.size(); index++)
      m_list.push_back(candidates[index]);
  }

 protected:
  vector<string> m_list;
};

class PVCandidateState {
 public:
  enum State {
    Ignored = 0,
    Canceled = 1,
    ReachedPageBoundary = 2,
    UpdatePage = 3,
    UpdateCandidateHighlight = 4,
    CandidateChosen = 5,
    InvalidCandidateKey = 6
  };
};

class PVOneDimensionalCandidatePanel : public OVOneDimensionalCandidatePanel {
 protected:
  bool m_visible;
  bool m_shouldUpdate;
  bool m_allowsPageWrapping;
  bool m_inControl;

  string m_prompt;

  PVCandidateList m_candidateList;
  size_t m_candidatesPerPage;
  size_t m_currentPage;
  OVKeyVector m_candidateKeys;
  OVKeyVector m_nextPageKeys;
  OVKeyVector m_previousPageKeys;
  OVKeyVector m_nextCandidateKeys;
  OVKeyVector m_previousCandidateKeys;
  OVKeyVector m_cancelKeys;
  OVKeyVector m_chooseHighlightedCandidateKeys;

  OVKeyVector m_defaultCandidateKeys;
  OVKeyVector m_defaultChooseHighlightedCandidateKeys;
  OVKeyVector m_defaultNextPageKeys;
  OVKeyVector m_defaultPreviousPageKeys;
  OVKeyVector m_defaultNextCandidateKeys;
  OVKeyVector m_defaultPreviousCandidateKeys;
  OVKeyVector m_defaultCancelKeys;

  size_t m_currentHighlightedCandidateIndex;
  size_t m_chosenCandidateIndex;
  string m_chosenCandidateString;

 protected:
  bool isKeyInVector(const OVKey& key, OVKeyVector& keyVector) {
    OVKeyVector::iterator it = keyVector.begin();

    for (; it != keyVector.end(); it++) {
      if (key == *it) return true;
    }

    return false;
  }

  size_t indexOfKeyInVector(const OVKey& key, const OVKeyVector& keyVector) {
    size_t index;
    for (index = 0; index < keyVector.size(); index++)
      if (key == keyVector[index]) return index;

    return 0;
  }

 public:
  bool isInControl() { return m_inControl; }

  void reset() {
    m_shouldUpdate = false;
    m_visible = false;
    m_prompt = "";
    m_candidateList.clear();
    m_currentPage = 0;
    m_inControl = false;
    m_chosenCandidateString = "";
    m_chosenCandidateIndex = 0;
    m_currentHighlightedCandidateIndex = 0;
    m_candidatesPerPage = 10;
    setToDefaultKeys();
  }

  void setShouldUpdate(bool shouldUpdate) { m_shouldUpdate = shouldUpdate; }

  bool shouldUpdate() { return m_shouldUpdate; }

  void stopControl() { m_inControl = false; }

  void finishUpdate() { m_shouldUpdate = false; }

  size_t chosenCandidateIndex() const { return m_chosenCandidateIndex; }

  const string chosenCandidateString() const { return m_chosenCandidateString; }

  size_t lastCurrentPageCandidateIndex() {
    size_t count = currentPageCandidateCount();
    return count > 0 ? count - 1 : 0;
  }

  PVCandidateState::State handleKeyEvent(const OVKey& key,
                                         OVLoaderService* loaderService) {
    if (isKeyInVector(key, m_cancelKeys)) {
      reset();
      return PVCandidateState::Canceled;
    }

    if (isKeyInVector(key, m_chooseHighlightedCandidateKeys)) {
      m_chosenCandidateIndex = m_currentPage * m_candidatesPerPage +
                               m_currentHighlightedCandidateIndex;
      m_chosenCandidateString =
          m_candidateList.candidateAtIndex(m_chosenCandidateIndex);
      return PVCandidateState::CandidateChosen;
    }

    if (isKeyInVector(key, m_candidateKeys)) {
      if (!currentPageCandidateCount())
        return PVCandidateState::InvalidCandidateKey;

      size_t index = indexOfKeyInVector(key, m_candidateKeys);

      if (index > lastCurrentPageCandidateIndex())
        return PVCandidateState::InvalidCandidateKey;

      m_chosenCandidateIndex = m_currentPage * m_candidatesPerPage + index;
      m_chosenCandidateString =
          m_candidateList.candidateAtIndex(m_chosenCandidateIndex);
      return PVCandidateState::CandidateChosen;
    }

    bool directionForward = false;
    bool changeHighlight = false;
    size_t oldCandidateIndex = m_currentHighlightedCandidateIndex;
    size_t oldPageIndex = m_currentPage;

    if (isKeyInVector(key, m_nextCandidateKeys)) {
      changeHighlight = true;
      directionForward = true;
    }

    if (isKeyInVector(key, m_previousCandidateKeys)) {
      changeHighlight = true;
      directionForward = false;
    }

    if (changeHighlight) {
      if (directionForward) {
        m_currentHighlightedCandidateIndex++;

        if (m_currentHighlightedCandidateIndex >
            lastCurrentPageCandidateIndex()) {
          goToNextPage();

          // see if we'v really flipped the page
          if (m_currentPage != oldPageIndex) {
            m_currentHighlightedCandidateIndex = 0;
          } else {
            // if not, either the list is empty, or page wrapping is off, we
            // have to rollback
            m_currentHighlightedCandidateIndex = oldCandidateIndex;
          }
        }
      } else {
        if (m_currentHighlightedCandidateIndex > 0) {
          m_currentHighlightedCandidateIndex--;
        } else {
          // we need to flip to the previous the page; see if we can do it
          goToPreviousPage();
          if (m_currentPage != oldPageIndex) {
            // flipped, so we set the candidate highlight to the last item of
            // the new page
            m_currentHighlightedCandidateIndex =
                lastCurrentPageCandidateIndex();
          } else {
            // failed--page wrapping is off or list is empty--now we rollback
            // (actually do nothing)
            ;
          }
        }
      }

      if (m_currentPage != oldPageIndex) return PVCandidateState::UpdatePage;

      if (m_currentHighlightedCandidateIndex != oldCandidateIndex)
        return PVCandidateState::UpdateCandidateHighlight;

      return PVCandidateState::ReachedPageBoundary;
    }

    bool changePage = false;
    directionForward = false;

    if (isKeyInVector(key, m_nextPageKeys)) {
      changePage = true;
      directionForward = true;
    }

    if (isKeyInVector(key, m_previousPageKeys)) {
      changePage = true;
      directionForward = false;
    }

    if (changePage) {
      size_t oldPageIndex = m_currentPage;

      if (directionForward) {
        goToNextPage();
        m_currentHighlightedCandidateIndex = 0;
      } else {
        goToPreviousPage();
        m_currentHighlightedCandidateIndex = 0;
      }

      if (m_currentPage != oldPageIndex) return PVCandidateState::UpdatePage;

      return PVCandidateState::ReachedPageBoundary;
    }

    return PVCandidateState::Ignored;
  }

 public:
  PVOneDimensionalCandidatePanel()
      : m_visible(false),
        m_shouldUpdate(false),
        m_candidatesPerPage(10),
        m_currentPage(0),
        m_allowsPageWrapping(true),
        m_inControl(false),
        m_currentHighlightedCandidateIndex(m_candidatesPerPage),
        m_chosenCandidateIndex(0),
        m_prompt("") {}

  virtual void hide() { m_visible = false; }

  virtual void show() { m_visible = true; }

  virtual void updateDisplay() { m_shouldUpdate = true; }

  virtual bool isVisible() { return m_visible; }

  virtual void setPrompt(const string& prompt) { m_prompt = prompt; }
  virtual string prompt() { return m_prompt; }

  virtual bool yieldToCandidateEventHandler() {
    if (m_candidateList.size()) {
      // yield control to candidate handler, so we start using hightlight
      m_currentHighlightedCandidateIndex = 0;
      m_inControl = true;
      return true;
    }

    return false;
  }

  virtual void cancelEventHandler() {
    m_currentHighlightedCandidateIndex = m_candidatesPerPage;
    m_inControl = false;
  }

  virtual bool isOneDimensionalPanel() { return true; }

  virtual bool isHorizontal() const { return false; }

  virtual bool isVertical() const { return false; }

  virtual OVCandidateList* candidateList() { return &m_candidateList; }

  virtual size_t candidatesPerPage() const { return m_candidatesPerPage; }

  virtual void setCandidatesPerPage(size_t number) {
    if (number > 0 && number <= m_candidateKeys.size())
      m_candidatesPerPage = number;
  }

  virtual size_t lastPage() const {
    size_t size = m_candidateList.size();

    if (!size) return 0;

    if (size % m_candidatesPerPage) return size / m_candidatesPerPage;

    return (size / m_candidatesPerPage) - 1;
  }

  virtual size_t pageCount() const {
    if (!m_candidateList.size()) return 0;

    return lastPage() + 1;
  }

  virtual size_t currentPage() const { return m_currentPage; }

  size_t currentPageCandidateCount() const {
    if (m_currentPage < lastPage()) {
      return m_candidatesPerPage;
    }

    return m_candidateList.size() - lastPage() * m_candidatesPerPage;
  }

  virtual bool allowsPageWrapping() const { return m_allowsPageWrapping; }

  virtual void setAllowsPageWrapping(bool allowsPageWrapping) {
    m_allowsPageWrapping = allowsPageWrapping;
  }

  virtual size_t currentHightlightIndex() const {
    return m_currentHighlightedCandidateIndex;
  }

  virtual void setHighlightIndex(size_t index) {
    m_currentHighlightedCandidateIndex = index;

    if (m_currentHighlightedCandidateIndex >= m_candidatesPerPage)
      m_currentHighlightedCandidateIndex = m_candidatesPerPage;
  }

  virtual size_t currentHightlightIndexInCandidateList() const {
    if (m_currentHighlightedCandidateIndex >= m_candidatesPerPage) return 0;

    return m_currentPage * m_candidatesPerPage +
           m_currentHighlightedCandidateIndex;
  }

  virtual size_t goToNextPage() {
    if (pageCount() > 0 && m_currentPage < lastPage()) {
      m_currentPage++;
    } else {
      if (m_allowsPageWrapping) {
        m_currentPage = 0;
      }
    }
    return m_currentPage;
  }

  virtual size_t goToPreviousPage() {
    if (m_currentPage > 0) {
      m_currentPage--;
    } else {
      if (m_allowsPageWrapping && pageCount() > 0) {
        m_currentPage = lastPage();
      }
    }

    return m_currentPage;
  }

  virtual size_t goToPage(size_t page) {
    if (page <= lastPage()) {
      m_currentPage = page;
    }

    return m_currentPage;
  }

  virtual const OVKey candidateKeyAtIndex(size_t index) {
    return index < m_candidateKeys.size() ? m_candidateKeys[index] : OVKey();
  }

  virtual void setCandidateKeys(const OVKeyVector& keys) {
    m_candidateKeys = keys;
  }

  virtual void setNextPageKeys(const OVKeyVector& keys) {
    m_nextPageKeys = keys;
  }

  virtual void setPreviousPageKeys(const OVKeyVector& keys) {
    m_previousPageKeys = keys;
  }

  virtual void setNextCandidateKeys(const OVKeyVector& keys) {
    m_nextCandidateKeys = keys;
  }

  virtual void setPreviousCandidateKeys(const OVKeyVector& keys) {
    m_previousCandidateKeys = keys;
  }

  virtual void setCancelKeys(const OVKeyVector& keys) { m_cancelKeys = keys; }

  virtual void setChooseHighlightedCandidateKeys(const OVKeyVector& keys) {
    m_chooseHighlightedCandidateKeys = keys;
  }

  virtual const OVKeyVector defaultCandidateKeys() const {
    return m_defaultCandidateKeys;
  }

  virtual const OVKeyVector defaultNextPageKeys() const {
    return m_defaultNextPageKeys;
  }

  virtual const OVKeyVector defaultNextCandidateKeys() const {
    return m_defaultNextCandidateKeys;
  }

  virtual const OVKeyVector defaultPreviousPageKeys() const {
    return m_defaultPreviousPageKeys;
  }

  virtual const OVKeyVector defaultPreviousCandidateKeys() const {
    return m_defaultPreviousCandidateKeys;
  }

  virtual const OVKeyVector defaultCancelKeys() const {
    return m_defaultCancelKeys;
  }

  virtual const OVKeyVector defaultChooseHighlightedCandidateKeys() const {
    return m_defaultChooseHighlightedCandidateKeys;
  }

 protected:
  virtual void populateDefaultCandidateKeys(OVLoaderService* loaderService) {
    string defaultCandidates = "1234567890";
    for (size_t index = 0; index < defaultCandidates.length(); index++)
      m_defaultCandidateKeys.push_back(
          loaderService->makeOVKey(defaultCandidates[index]));
  }

  virtual void populateDefaultCancelKeys(OVLoaderService* loaderService) {
    m_defaultCancelKeys.push_back(loaderService->makeOVKey(OVKeyCode::Esc));
  }

  virtual void populateDefaultChooseHighlightedCandidateKeys(
      OVLoaderService* loaderService) {
    m_defaultChooseHighlightedCandidateKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::Return));
  }

  virtual void setToDefaultKeys() {
    setCandidateKeys(defaultCandidateKeys());
    setNextCandidateKeys(defaultNextCandidateKeys());
    setPreviousCandidateKeys(defaultPreviousCandidateKeys());
    setNextPageKeys(defaultNextPageKeys());
    setPreviousPageKeys(defaultPreviousPageKeys());
    setCancelKeys(defaultCancelKeys());
    setChooseHighlightedCandidateKeys(defaultChooseHighlightedCandidateKeys());
  }

  virtual void populateAndSetDefaultKeys(OVLoaderService* loaderService) {
    populateDefaultCandidateKeys(loaderService);
    populateDefaultCancelKeys(loaderService);
    populateDefaultChooseHighlightedCandidateKeys(loaderService);
  }
};

class PVHorizontalCandidatePanel : public PVOneDimensionalCandidatePanel {
 public:
  PVHorizontalCandidatePanel(OVLoaderService* loaderService) {
    populateAndSetDefaultKeys(loaderService);
    m_defaultNextCandidateKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::Right));
    m_defaultPreviousCandidateKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::Left));
    m_defaultNextPageKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::PageDown));
    m_defaultNextPageKeys.push_back(loaderService->makeOVKey(OVKeyCode::Down));
    m_defaultNextPageKeys.push_back(loaderService->makeOVKey(OVKeyCode::Space));
    m_defaultPreviousPageKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::PageUp));
    m_defaultPreviousPageKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::Up));
    setToDefaultKeys();
  }

  virtual bool isHorizontal() const { return true; }
};

class PVVerticalCandidatePanel : public PVOneDimensionalCandidatePanel {
 public:
  PVVerticalCandidatePanel(OVLoaderService* loaderService) {
    populateAndSetDefaultKeys(loaderService);
    PVOneDimensionalCandidatePanel::populateAndSetDefaultKeys(loaderService);
    m_defaultNextCandidateKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::Down));
    m_defaultPreviousCandidateKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::Up));
    m_defaultNextPageKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::PageDown));
    m_defaultNextPageKeys.push_back(loaderService->makeOVKey(OVKeyCode::Space));
    m_defaultPreviousPageKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::PageUp));
    m_defaultNextPageKeys.push_back(loaderService->makeOVKey(OVKeyCode::Right));
    m_defaultPreviousPageKeys.push_back(
        loaderService->makeOVKey(OVKeyCode::Left));
    setToDefaultKeys();
  }

  virtual bool isVertical() const { return true; }
};

class PVFreeContentStorage : public OVFreeContentStorage {
 public:
  virtual void clear() { m_content = ""; }

  virtual void setContent(const string& content) { m_content = content; }

  virtual void appendContent(const string& content) { m_content += content; }

  virtual const string content() const { return m_content; }

 public:
  string m_content;
};

class PVPlainTextCandidatePanel : public OVPlainTextCandidatePanel {
 public:
  PVPlainTextCandidatePanel() : m_visible(false), m_shouldUpdate(false) {}

  virtual OVFreeContentStorage* textStorage() { return &m_textStorage; }

  virtual PVFreeContentStorage* accessTextStorage() { return &m_textStorage; }

  virtual void hide() { m_visible = false; }

  virtual void show() { m_visible = true; }

  virtual void setPrompt(const string& prompt) { m_prompt = prompt; }
  virtual string prompt() { return m_prompt; }

  virtual void updateDisplay() { m_shouldUpdate = true; }
  virtual bool isVisible() { return m_visible; }

  virtual bool yieldToCandidateEventHandler() { return false; }

  virtual void cancelEventHandler() {}

 public:
  void reset() {
    m_shouldUpdate = false;
    m_visible = false;
    m_prompt = "";
    m_textStorage.clear();
  }

  void finishUpdate() { m_shouldUpdate = false; }

  bool shouldUpdate() { return m_shouldUpdate; }

 protected:
  bool m_visible;
  bool m_shouldUpdate;
  string m_prompt;
  PVFreeContentStorage m_textStorage;
};

class PVCandidateService : public OVCandidateService {
 public:
  PVCandidateService(OVLoaderService* loaderService)
      : m_verticalCandidatePanel(loaderService),
        m_horizontalCandidatePanel(loaderService),
        m_oneDimensionalPanelIsVertical(true) {}

  virtual OVPlainTextCandidatePanel* usePlainTextCandidatePanel() {
    m_lastUsedPanel = &m_plainTextCandidatePanel;
    return &m_plainTextCandidatePanel;
  }

  virtual OVOneDimensionalCandidatePanel* useVerticalCandidatePanel() {
    m_lastUsedPanel = &m_verticalCandidatePanel;
    return &m_verticalCandidatePanel;
  }

  virtual OVOneDimensionalCandidatePanel* useHorizontalCandidatePanel() {
    m_lastUsedPanel = &m_horizontalCandidatePanel;
    return &m_horizontalCandidatePanel;
  }

  virtual OVOneDimensionalCandidatePanel* useOneDimensionalCandidatePanel() {
    return m_oneDimensionalPanelIsVertical ? useVerticalCandidatePanel()
                                           : useHorizontalCandidatePanel();
  }

 public:
  virtual void resetAll() {
    m_horizontalCandidatePanel.reset();
    m_verticalCandidatePanel.reset();
    m_plainTextCandidatePanel.reset();
    m_lastUsedPanel = 0;
  }

  virtual void resetExcept(OVCandidatePanel* panel) {
    if (panel != &m_horizontalCandidatePanel)
      m_horizontalCandidatePanel.reset();

    if (panel != &m_verticalCandidatePanel) m_verticalCandidatePanel.reset();

    if (panel != &m_plainTextCandidatePanel) m_plainTextCandidatePanel.reset();
  }

  virtual OVCandidatePanel* lastUsedPanel() { return m_lastUsedPanel; }

  virtual PVVerticalCandidatePanel* accessVerticalCandidatePanel() {
    return &m_verticalCandidatePanel;
  }

  virtual PVHorizontalCandidatePanel* accessHorizontalCandidatePanel() {
    return &m_horizontalCandidatePanel;
  }

  virtual PVPlainTextCandidatePanel* accessPlainTextCandidatePanel() {
    return &m_plainTextCandidatePanel;
  }

 public:
  virtual void setOneDimensionalPanelVertical(bool vertical = true) {
    m_oneDimensionalPanelIsVertical = vertical;
  }

 protected:
  bool m_oneDimensionalPanelIsVertical;
  PVPlainTextCandidatePanel m_plainTextCandidatePanel;
  PVHorizontalCandidatePanel m_horizontalCandidatePanel;
  PVVerticalCandidatePanel m_verticalCandidatePanel;
  OVCandidatePanel* m_lastUsedPanel;
};
};  // namespace OpenVanilla

#endif