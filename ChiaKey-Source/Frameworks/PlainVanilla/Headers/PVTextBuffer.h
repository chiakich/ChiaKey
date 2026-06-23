//
// PVTextBuffer.h
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

#ifndef PVTextBuffer_h
#define PVTextBuffer_h

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

namespace OpenVanilla {
using namespace std;

class PVCombinedUTF16TextBuffer;

class PVTextBuffer : public OVTextBuffer {
 public:
  // methods for loaders
  virtual bool shouldUpdate();
  virtual bool isCommitted();
  virtual void finishUpdate();
  virtual void finishCommit();
  virtual const OVTextBuffer::RangePair& highlightMark() const;
  virtual const vector<OVTextBuffer::RangePair>& wordSegments() const;
  virtual const pair<string, string> composedTextsSeparatedByCursor() const;
  virtual const vector<string>& codePoints() const;
  virtual void setComposedCommittedText(const string& text);
  virtual const string toolTipText() const;

 public:
  PVTextBuffer();
  virtual void clear();
  virtual void setText(const string& text);
  virtual void appendText(const string& text, bool moveCursor = true);

  virtual void commit();
  virtual void commitAsTextSegment();

  virtual void commit(const string& text);
  virtual void commitAsTextSegment(const string& text);

  virtual void updateDisplay();
  virtual bool isEmpty() const;
  virtual size_t codePointCount() const;
  virtual const string codePointAt(size_t index) const;
  virtual const string composedText() const;
  virtual const string composedCommittedText() const;
  virtual const vector<string> composedCommittedTextSegments() const;

  virtual void setCursorPosition(size_t position);
  virtual size_t cursorPosition() const;
  virtual void showToolTip(const string& text);
  virtual void clearToolTip();

  virtual void setHighlightMark(const OVTextBuffer::RangePair& range);
  virtual void setWordSegments(const vector<OVTextBuffer::RangePair>& segments);

  virtual void setSuggestedReadingTextStyle(ReadingTextStyle style);
  virtual ReadingTextStyle defaultReadingTextStyle() const;

 protected:
  // utility functions
  virtual RangePair normalizedRangePair(const RangePair& range);

 protected:
  bool m_stateChanged;
  bool m_isCommitted;
  vector<string> m_codePoints;
  size_t m_cursorPosition;
  OVTextBuffer::RangePair m_highlightMark;
  vector<OVTextBuffer::RangePair> m_wordSegments;
  string m_toolTip;
  string m_composedCommittedText;
  vector<string> m_composedCommittedTextSegments;
  OVTextBuffer::ReadingTextStyle m_readingTextStyle;
};

inline bool PVTextBuffer::shouldUpdate() { return m_stateChanged; }

inline bool PVTextBuffer::isCommitted() { return m_isCommitted; }

inline void PVTextBuffer::finishUpdate() { m_stateChanged = false; }

inline void PVTextBuffer::finishCommit() {
  m_isCommitted = false;
  m_composedCommittedText = "";
  m_composedCommittedTextSegments.clear();
}

inline const OVTextBuffer::RangePair& PVTextBuffer::highlightMark() const {
  return m_highlightMark;
}

inline const vector<OVTextBuffer::RangePair>& PVTextBuffer::wordSegments()
    const {
  return m_wordSegments;
}

inline const pair<string, string> PVTextBuffer::composedTextsSeparatedByCursor()
    const {
  vector<string> ante;
  vector<string> post;
  ante.insert(ante.begin(), m_codePoints.begin(),
              m_codePoints.begin() + m_cursorPosition);
  post.insert(post.begin(), m_codePoints.begin() + m_cursorPosition,
              m_codePoints.end());
  return pair<string, string>(OVUTF8Helper::CombineCodePoints(ante),
                              OVUTF8Helper::CombineCodePoints(post));
}

inline const vector<string>& PVTextBuffer::codePoints() const {
  return m_codePoints;
}

inline void PVTextBuffer::setComposedCommittedText(const string& text) {
  m_composedCommittedText = text;
  m_composedCommittedTextSegments.clear();
  m_composedCommittedTextSegments.push_back(text);
  m_isCommitted = true;
}

inline const string PVTextBuffer::toolTipText() const { return m_toolTip; }

inline PVTextBuffer::PVTextBuffer()
    : m_stateChanged(false),
      m_isCommitted(false),
      m_cursorPosition(0),
      m_highlightMark(OVTextBuffer::RangePair(0, 0)),
      m_readingTextStyle(Horizontal) {}

inline void PVTextBuffer::clear() {
  m_stateChanged = false;
  m_isCommitted = false;
  m_cursorPosition = 0;
  m_highlightMark = OVTextBuffer::RangePair(0, 0);
  m_codePoints.clear();
  m_wordSegments.clear();
  m_toolTip = "";
  m_readingTextStyle = Horizontal;

  // we don't clear m_composedCommittedText here because: the use case might be
  // b->commit(); b->setText("new text"); m_composedCommittedText should be
  // cleared using finishCommit();

  // m_composedCommittedText = "";
}

inline void PVTextBuffer::setText(const string& text) {
  if (text != composedText()) {
    m_codePoints = OVUTF8Helper::SplitStringByCodePoint(text);
    m_cursorPosition = m_codePoints.size();
    m_stateChanged = true;
  }
}

inline void PVTextBuffer::appendText(const string& text, bool moveCursor) {
  if (text.length()) {
    vector<string> insertedCodePoints =
        OVUTF8Helper::SplitStringByCodePoint(text);
    m_codePoints.insert(m_codePoints.end(), insertedCodePoints.begin(),
                        insertedCodePoints.end());

    if (moveCursor) m_cursorPosition = m_codePoints.size();

    m_stateChanged = true;
  }
}

inline void PVTextBuffer::commit() {
  string composedTextString = composedText();
  m_composedCommittedText += composedTextString;

  if (m_composedCommittedTextSegments.size()) {
    m_composedCommittedTextSegments.back() += composedTextString;
  } else {
    m_composedCommittedTextSegments.push_back(composedTextString);
  }

  string tmp = m_toolTip;
  clear();
  m_toolTip = tmp;
  m_stateChanged = true;
  m_isCommitted = true;
}

inline void PVTextBuffer::commitAsTextSegment() {
  string composedTextString = composedText();
  m_composedCommittedText += composedTextString;
  m_composedCommittedTextSegments.push_back(composedTextString);
  string tmp = m_toolTip;
  clear();
  m_toolTip = tmp;
  m_stateChanged = true;
  m_isCommitted = true;
}

inline void PVTextBuffer::commit(const string& text) {
  m_composedCommittedText += text;

  if (m_composedCommittedTextSegments.size()) {
    m_composedCommittedTextSegments.back() += text;
  } else {
    m_composedCommittedTextSegments.push_back(text);
  }

  m_stateChanged = true;
  m_isCommitted = true;
}

inline void PVTextBuffer::commitAsTextSegment(const string& text) {
  m_composedCommittedText += text;
  m_composedCommittedTextSegments.push_back(text);

  m_stateChanged = true;
  m_isCommitted = true;
}

inline void PVTextBuffer::updateDisplay() { m_stateChanged = true; }

inline bool PVTextBuffer::isEmpty() const { return m_codePoints.size() == 0; }

inline size_t PVTextBuffer::codePointCount() const {
  return m_codePoints.size();
}

inline const string PVTextBuffer::codePointAt(size_t index) const {
  return (index >= m_codePoints.size()) ? string() : m_codePoints[index];
}

inline const string PVTextBuffer::composedText() const {
  return OVUTF8Helper::CombineCodePoints(m_codePoints);
}

inline const string PVTextBuffer::composedCommittedText() const {
  return m_composedCommittedText;
}

inline const vector<string> PVTextBuffer::composedCommittedTextSegments()
    const {
  return m_composedCommittedTextSegments;
}

inline void PVTextBuffer::setCursorPosition(size_t position) {
  if (position <= m_codePoints.size()) {
    m_cursorPosition = position;
    m_stateChanged = true;
  }
}

inline size_t PVTextBuffer::cursorPosition() const { return m_cursorPosition; }

inline void PVTextBuffer::showToolTip(const string& text) {
  if (text != m_toolTip) {
    m_toolTip = text;
    m_stateChanged = true;
  }
}

inline void PVTextBuffer::clearToolTip() {
  if (m_toolTip.length()) {
    m_toolTip = "";
    m_stateChanged = true;
  }
}

inline void PVTextBuffer::setHighlightMark(
    const OVTextBuffer::RangePair& range) {
  OVTextBuffer::RangePair normalized = normalizedRangePair(range);
  if (normalized != m_highlightMark) {
    m_highlightMark = range;
    m_stateChanged = true;
  }
}

inline void PVTextBuffer::setWordSegments(
    const vector<OVTextBuffer::RangePair>& segments) {
  vector<OVTextBuffer::RangePair> result;
  for (vector<OVTextBuffer::RangePair>::const_iterator iter = segments.begin();
       iter != segments.end(); ++iter)
    result.push_back(normalizedRangePair(*iter));

  if (result != m_wordSegments) {
    m_wordSegments = result;
    m_stateChanged = true;
  }
}

inline void PVTextBuffer::setSuggestedReadingTextStyle(
    OVTextBuffer::ReadingTextStyle style) {
  m_readingTextStyle = style;
}

inline OVTextBuffer::ReadingTextStyle PVTextBuffer::defaultReadingTextStyle()
    const {
  return m_readingTextStyle;
}

inline OVTextBuffer::RangePair PVTextBuffer::normalizedRangePair(
    const OVTextBuffer::RangePair& range) {
  OVTextBuffer::RangePair result = range;
  size_t size = m_codePoints.size();
  if (result.first > size) result.first = size;

  if (result.first + result.second > size) result.second = size - result.first;

  return result;
}

class PVCombinedUTF16TextBuffer {
 public:
  typedef int SegmentType;

  enum { Unmarked = 0, Reading = 1, Highlight = 2, WordSegment = 3 };

  typedef pair<OVTextBuffer::RangePair, SegmentType> SegmentPair;
  typedef vector<SegmentPair> SegmentPairVector;

  // OS X needs to set the two optional parameters to true, because: 1. cursor
  // mode and highlight mode are mutually exclusive; 2. there is only one
  // highlight ("bold-underlined text") per buffer, and all other word segments
  // are thinly underlined
  PVCombinedUTF16TextBuffer(const PVTextBuffer& composingText,
                            const PVTextBuffer& readingText,
                            bool retainsReadingOnly = false,
                            bool unmarkedAsWordSegment = false);

  const wstring& wideComposedText() const { return m_wideComposedText; }

  const string& composedText() const { return m_composedText; }

  const SegmentPairVector& segmentPairs() const { return m_segmentPairs; }

  const SegmentPairVector& wideSegmentPairs() const {
    return m_wideSegmentPairs;
  }

  size_t cursorPosition() const { return m_cursorPosition; }

  size_t wideCursorPosition() const { return m_wideCursorPosition; }

  friend ostream& operator<<(ostream& stream,
                             PVCombinedUTF16TextBuffer& buffer);

 protected:
  size_t m_skipStart;
  size_t m_skipLength;

  vector<size_t> m_wideComposedTextCursorMap;
  size_t m_wideCursorPosition;
  size_t m_cursorPosition;
  wstring m_wideComposedText;
  string m_composedText;
  vector<pair<OVTextBuffer::RangePair, SegmentType> > m_segmentPairs;
  vector<pair<OVTextBuffer::RangePair, SegmentType> > m_wideSegmentPairs;

  size_t indexAfterSkipping(size_t index);

  // make an cursor map so that map[position x in UTF-8 string] = position y in
  // UTF-16 string
  static vector<size_t> MakeCursorMap(const vector<string>& codePoints);
};

inline ostream& operator<<(ostream& stream, PVCombinedUTF16TextBuffer& buffer) {
  const char* types[4] = {"Unmarked", "Reading", "Highlight", "WordSegment"};
  stream << "PVCombinedUTF16TextBuffer:" << endl;
  stream << "Cursor position: " << buffer.m_cursorPosition
         << " (wide: " << buffer.m_wideCursorPosition << ")" << endl;
  stream << "Composed text: " << buffer.m_composedText
         << " (wide: " << OVUTF8::FromUTF16(buffer.m_wideComposedText) << ")"
         << endl;
  stream << "Segment pairs:" << endl;
  size_t index, size = buffer.m_segmentPairs.size();

  for (index = 0; index < size; index++) {
    pair<OVTextBuffer::RangePair, PVCombinedUTF16TextBuffer::SegmentType> seg =
        buffer.m_segmentPairs[index];
    stream << "    Pair " << index << ": range (" << seg.first.first << ", "
           << seg.first.second << ") type " << types[seg.second] << " == ";

    seg = buffer.m_wideSegmentPairs[index];
    stream << "wide range (" << seg.first.first << ", " << seg.first.second
           << ") type " << types[seg.second] << endl;
  }

  return stream;
}

inline vector<size_t> PVCombinedUTF16TextBuffer::MakeCursorMap(
    const vector<string>& codePoints) {
  size_t u16index = 0;

  vector<size_t> result;
  result.push_back(u16index);

  for (vector<string>::const_iterator iter = codePoints.begin();
       iter != codePoints.end(); ++iter) {
    unsigned int v = OVUTF8Helper::CodePointFromSingleUTF8String(*iter);

    if (v < 0x10000) {
      result.push_back(++u16index);
    } else {
      u16index += 2;
      result.push_back(u16index);
    }
  }

  return result;
}

inline size_t PVCombinedUTF16TextBuffer::indexAfterSkipping(size_t index) {
  return (index < m_skipStart) ? index : index + m_skipLength;
}

inline PVCombinedUTF16TextBuffer::PVCombinedUTF16TextBuffer(
    const PVTextBuffer& composingText, const PVTextBuffer& readingText,
    bool readingOverride, bool unmarkedAsWordSegment) {
  pair<string, string> splitComposingText =
      composingText.composedTextsSeparatedByCursor();
  m_composedText = splitComposingText.first + readingText.composedText() +
                   splitComposingText.second;
  m_wideComposedText = OVUTF16::FromUTF8(m_composedText);

  m_wideComposedTextCursorMap = PVCombinedUTF16TextBuffer::MakeCursorMap(
      OVUTF8Helper::SplitStringByCodePoint(m_composedText));
  m_wideCursorPosition =
      m_wideComposedTextCursorMap[composingText.cursorPosition() +
                                  readingText.cursorPosition()];

  m_cursorPosition =
      composingText.cursorPosition() + readingText.cursorPosition();

  m_skipStart = composingText.cursorPosition();
  m_skipLength = readingText.codePointCount();

  vector<pair<size_t, SegmentType> > segmentMap;
  for (size_t index = 0;
       index < composingText.codePointCount() + readingText.codePointCount();
       ++index)
    segmentMap.push_back(pair<int, SegmentType>(0, Unmarked));

  size_t filler;
  size_t counter = 1;
  // first, we fill in the word segments
  for (vector<OVTextBuffer::RangePair>::const_iterator iter =
           composingText.wordSegments().begin();
       iter != composingText.wordSegments().end(); ++iter) {
    for (filler = (*iter).first; filler < (*iter).first + (*iter).second;
         filler++) {
      segmentMap[indexAfterSkipping(filler)].first = counter;
      segmentMap[indexAfterSkipping(filler)].second = WordSegment;
    }

    counter++;
  }

  // then the highlight (and make sure we only have one consecutive highlight
  OVTextBuffer::RangePair highlight = composingText.highlightMark();
  SegmentType fillType = Highlight;
  for (filler = highlight.first; filler < highlight.first + highlight.second;
       filler++) {
    segmentMap[indexAfterSkipping(filler)].first = counter;
    segmentMap[indexAfterSkipping(filler)].second = fillType;

    if (filler + 1 < highlight.first + highlight.second) {
      if (indexAfterSkipping(filler + 1) > filler + 1) {
        counter++;
        fillType = WordSegment;
      }
    }
  }
  counter++;

  bool hasReading = (readingText.codePointCount() != 0);

  // then we overlap the reading
  for (filler = 0; filler < readingText.codePointCount(); ++filler) {
    segmentMap[m_skipStart + filler].first = counter;
    segmentMap[m_skipStart + filler].second = Reading;
  }

  // now we gather everything
  vector<pair<size_t, SegmentType> >::iterator miter = segmentMap.begin();
  while (miter != segmentMap.end()) {
    vector<pair<size_t, SegmentType> >::iterator iterFrom = miter;
    size_t headNumber = (*miter).first;
    SegmentType type = (*miter).second;

    if (unmarkedAsWordSegment && type == Unmarked) type = WordSegment;

    // OS X style, if there is reading, there is no highlight, no word segment,
    // no nothing, yadda yadda
    if (hasReading && readingOverride) type = Unmarked;

    while (miter != segmentMap.end()) {
      if ((*miter).first != headNumber) break;

      miter++;
    }

    // if (type != Unmarked) {
    if (true) {
      size_t from = iterFrom - segmentMap.begin();
      size_t length = miter - iterFrom;
      m_segmentPairs.push_back(
          SegmentPair(OVTextBuffer::RangePair(from, length), type));

      size_t wideFrom = m_wideComposedTextCursorMap[from];
      size_t wideLength = m_wideComposedTextCursorMap[from + length] - wideFrom;
      m_wideSegmentPairs.push_back(
          SegmentPair(OVTextBuffer::RangePair(wideFrom, wideLength), type));
    }
  }
}

};  // namespace OpenVanilla

#endif