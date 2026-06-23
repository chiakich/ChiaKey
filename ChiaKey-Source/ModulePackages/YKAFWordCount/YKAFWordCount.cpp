/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "YKAFWordCount.h"

#include <sstream>

#ifdef WIN32
#define snprintf sprintf_s
#endif

namespace OpenVanilla {

void YKAFWordCountContext::stopSession(OVLoaderService* loaderService) {
  m_sessionStartTime = 0;
  m_sessionCharCount = 0;
}

void YKAFWordCountContext::startSession(OVLoaderService* loaderService) {
  m_sessionStartTime = OVDateTimeHelper::GetTimeIntervalSince1970();
  m_sessionCharCount = 0;
}

bool YKAFWordCountContext::handleDirectText(
    const string& text, OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVCandidateService* candidateService, OVLoaderService* loaderService) {
  size_t cpc = OVUTF8Helper::SplitStringByCodePoint(text).size();
  m_module->addSessionCharCount(m_sessionStartTime, cpc);
  // loaderService->logger("YKAFWordCount") << "Text : " << text << ",
  // codepoints: " << cpc << endl;
  return false;
}

const string YKAFWordCount::identifier() const {
  return string("YKAFWordCount");
}

void YKAFWordCount::saveConfig(OVKeyValueMap* moduleConfig,
                               OVLoaderService* loaderService) {
  // accumulate the total count
  size_t totalCount = moduleConfig->intValueForKey("TotalCount");
  // loaderService->logger("YKAFWordCount") << "total count was: " <<
  // totalCount;
  totalCount += m_latestSessionCharCount;
  // loaderService->logger("YKAFWordCount") << "total count now: " << totalCount
  // << endl;

  moduleConfig->setKeyIntValue("TotalCount", (int)totalCount);

  // get the weekly record
  time_t todayStart =
      OVDateTimeHelper::GetTimeIntervalSince1970AtBeginningOfTodayLocalTime();

  vector<string> weeklyRecord = OVStringHelper::Split(
      moduleConfig->stringValueForKey("WeeklyRecord"), ',');
  vector<string> newRecord;
  size_t s = weeklyRecord.size();
  size_t weeklyCount = 0, todayCount = 0;

  for (size_t i = 0; i < s; i += 2) {
    if (i + 1 >= s) {
      // not a pair
      break;
    }

    size_t count = atoi(weeklyRecord[i].c_str());
    string tss = weeklyRecord[i + 1];
    time_t timeStamp =
        OVDateTimeHelper::GetTimeIntervalSince1970FromString(tss);

    if (timeStamp < todayStart - 86400 * 7) {
      continue;
    }

    if (timeStamp == todayStart) {
      todayCount = count;
      continue;
    }

    stringstream sst;
    sst << count;
    newRecord.push_back(sst.str());
    newRecord.push_back(tss);

    weeklyCount += count;
  }

  todayCount += m_latestSessionCharCount;
  weeklyCount += todayCount;
  stringstream sst1, sst2, sst3;
  sst1 << todayCount;
  sst2 << todayStart;
  newRecord.push_back(sst1.str());
  ;
  newRecord.push_back(sst2.str());

  sst3 << weeklyCount;
  moduleConfig->setKeyStringValue("TodayCount", sst1.str());
  moduleConfig->setKeyStringValue("TodayTimestamp", sst2.str());
  moduleConfig->setKeyStringValue("WeeklyCount", sst3.str());
  moduleConfig->setKeyStringValue("WeeklyRecord",
                                  OVStringHelper::Join(newRecord, ","));

  m_latestSessionCharCount = 0;
}

};  // namespace OpenVanilla
