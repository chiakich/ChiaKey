//
// Developed for Yahoo! Taiwan by Lithoglyph Inc.
// Copyright (c) 2007-2010 Yahoo! Taiwan.
//

#include "OVAFSearch.h"

#include <sstream>

#ifdef WIN32
#define snprintf sprintf_s
#endif

#ifdef WIN32
#define OS "pc"
#else
#define OS "mac"
#endif

#define VERSION_FOR_SEARCH "2528"

namespace OpenVanilla {

void OVAFSearchContext::stopSession(OVLoaderService* loaderService) {
  m_logging = false;
  m_searchString = "";
  m_searchedCursor = m_seachedText.size();
  loaderService->setPrompt("");
  loaderService->setPromptDescription("");
  loaderService->setLog("");
}

void OVAFSearchContext::startSession(OVLoaderService* loaderService) {
  m_logging = false;
  m_searchString = "";
  m_searchedCursor = m_seachedText.size();
  loaderService->setPrompt("");
  loaderService->setPromptDescription("");
  loaderService->setLog("");
}

const string OVAFSearchContext::EscapeURLString(const string& text) {
  stringstream sst;

  for (string::const_iterator i = text.begin(); i != text.end(); ++i) {
    if ((*i >= '0' && *i <= '9') || (*i >= 'A' && *i <= 'Z') ||
        (*i >= 'a' && *i <= 'z')) {
      sst << (char)*i;
    } else {
      sst << '%';
      sst << hex;
      sst.width(2);
      sst.fill('0');
      unsigned char c = *i;
      sst << (unsigned int)c;
    }
  }

  return sst.str();
}

string OVAFSearchContext::searchWeb(const string& text,
                                    OVLoaderService* loaderService) {
  string url;

  if (loaderService->locale() == "zh_TW" ||
      loaderService->locale() == "zh-Hant") {
    // The URL of Yahoo! Search Taiwan
    // url = "http://tw.search.yahoo.com/search?fr=yfp&ei=UTF-8&p=";
    url = "http://tw.search.yahoo.com/search/combo?ei=UTF-8&fr=download&p=";
  } else if (loaderService->locale() == "zh_CN" ||
             loaderService->locale() == "zh-Hans") {
    // The URL of Yahoo! Search China
    // url = "http://search.yahoo.com/search?&y=Search&fr=sfp&ei=UTF-8&p=";
    url = "http://search.cn.yahoo.com/search?&y=Search&fr=sfp&ei=UTF-8&p=";
  } else {
    // The URL of Yahoo! Search
    // url = "http://search.yahoo.com/search?&y=Search&fr=sfp&ei=UTF-8&p=";
    url = "http://search.yahoo.com/search?&y=Search&fr=sfp&ei=UTF-8&p=";
  }

  url += EscapeURLString(text);

  loaderService->callLoaderFeature("SendTrackerRequest", "onekey-search");

  return url;
}

string OVAFSearchContext::searchStock(const string& text,
                                      OVLoaderService* loaderService) {
  string big5text = loaderService->encodingService()
                        ->convertEncoding("UTF-8", "BIG-5", text)
                        .second;
  string url =
      "http://tw.rd.yahoo.com/referurl/stock/index/get_quote/quote/*http://"
      "tw.stock.yahoo.com/h/stockmenu.php?func=G&stock_id=";
  url += EscapeURLString(big5text);
  loaderService->callLoaderFeature("SendTrackerRequest", "onekey-stock");
  return url;
}

string OVAFSearchContext::searchWretch(const string& text,
                                       OVLoaderService* loaderService) {
  string url =
      "http://tw.info.search.yahoo.com/search/"
      "wretch?f=&ei=UTF-8&fr=cb-wretch&searchtype=web&fr2=tab-web&q=";
  url += EscapeURLString(text);
  loaderService->callLoaderFeature("SendTrackerRequest", "onekey-wretch");
  return url;
}

string OVAFSearchContext::searchAuction(const string& text,
                                        OVLoaderService* loaderService) {
  string big5text = loaderService->encodingService()
                        ->convertEncoding("UTF-8", "BIG-5", text)
                        .second;
  string url = "http://tw.search.bid.yahoo.com/search/ac?p=";
  url += EscapeURLString(big5text);
  loaderService->callLoaderFeature("SendTrackerRequest", "onekey-auction");
  return url;
}

string OVAFSearchContext::searchMap(const string& text,
                                    OVLoaderService* loaderService) {
  string url = "http://tw.maps.yahoo.com/#pok%011%02pos%01";
  url += EscapeURLString(text);
  loaderService->callLoaderFeature("SendTrackerRequest", "onekey-map");
  return url;
}

void OVAFSearchContext::setPrompt(OVTextBuffer* composingText,
                                  OVLoaderService* loaderService) {
  if (!m_logging) return;
  string locale = loaderService->locale();
  vector<string> promptVector;
  if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
    promptVector.push_back("英漢字典：");
    promptVector.push_back("漢英字典：");
    promptVector.push_back("綜合搜尋：");
    promptVector.push_back("股市查詢：");
    promptVector.push_back("無名搜尋：");
    promptVector.push_back("拍賣搜尋：");
    promptVector.push_back("地圖搜尋：");
#else
    promptVector.push_back(
        "\xe8\x8b\xb1\xe6\xbc\xa2\xe5\xad\x97\xe5\x85\xb8\xef\xbc\x9a");
    promptVector.push_back(
        "\xe6\xbc\xa2\xe8\x8b\xb1\xe5\xad\x97\xe5\x85\xb8\xef\xbc\x9a");
    promptVector.push_back(
        "\xe7\xb6\x9c\xe5\x90\x88\xe6\x90\x9c\xe5\xb0\x8b\xef\xbc\x9a");
    promptVector.push_back(
        "\xe8\x82\xa1\xe5\xb8\x82\xe6\x9f\xa5\xe8\xa9\xa2\xef\xbc\x9a");
    promptVector.push_back(
        "\xe7\x84\xa1\xe5\x90\x8d\xe6\x90\x9c\xe5\xb0\x8b\xef\xbc\x9a");
    promptVector.push_back(
        "\xe6\x8b\x8d\xe8\xb3\xa3\xe6\x90\x9c\xe5\xb0\x8b\xef\xbc\x9a");
    promptVector.push_back(
        "\xe5\x9c\xb0\xe5\x9c\x96\xe6\x90\x9c\xe5\xb0\x8b\xef\xbc\x9a");
#endif
  } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
    promptVector.push_back("英汉字典：");
    promptVector.push_back("汉英字典：");
    promptVector.push_back("综合搜索：");
    promptVector.push_back("台湾股市：");
    promptVector.push_back("无名搜索：");
    promptVector.push_back("拍卖搜索：");
    promptVector.push_back("地图搜索：");
#else
    promptVector.push_back(
        "\xe8\x8b\xb1\xe6\xb1\x89\xe5\xad\x97\xe5\x85\xb8\xef\xbc\x9a");
    promptVector.push_back(
        "\xe6\xb1\x89\xe8\x8b\xb1\xe5\xad\x97\xe5\x85\xb8\xef\xbc\x9a");
    promptVector.push_back(
        "\xe7\xbb\xbc\xe5\x90\x88\xe6\x90\x9c\xe7\xb4\xa2\xef\xbc\x9a");
    promptVector.push_back(
        "\xe5\x8f\xb0\xe6\xb9\xbe\xe8\x82\xa1\xe5\xb8\x82\xef\xbc\x9a");
    promptVector.push_back(
        "\xe6\x97\xa0\xe5\x90\x8d\xe6\x90\x9c\xe7\xb4\xa2\xef\xbc\x9a");
    promptVector.push_back(
        "\xe6\x8b\x8d\xe5\x8d\x96\xe6\x90\x9c\xe7\xb4\xa2\xef\xbc\x9a");
    promptVector.push_back(
        "\xe5\x9c\xb0\xe5\x9b\xbe\xe6\x90\x9c\xe7\xb4\xa2\xef\xbc\x9a");
#endif
  } else {
    promptVector.push_back("English-Chinese Dictionary: ");
    promptVector.push_back("Chinese-English Dictionary: ");
    promptVector.push_back("Web: ");
    promptVector.push_back("Taiex (TW Stock): ");
    promptVector.push_back("Wretch.cc: ");
    promptVector.push_back("Yahoo! Taiwan Auction: ");
    promptVector.push_back("Yahoo! Taiwan Map: ");
  }

  loaderService->setPrompt(promptVector[m_searchType]);
  loaderService->setLog(m_searchString);
}

void OVAFSearchContext::setLog(OVTextBuffer* composingText,
                               OVLoaderService* loaderService) {
  if (!m_logging) return;
  setPrompt(composingText, loaderService);
  string locale = loaderService->locale();
  vector<string> localizedSearchTypeNames;

  if (m_searchType == 0) {
    if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
      loaderService->setPromptDescription(
          string("請輸入您想查詢單字，按 Enter 開始搜尋。\n按 Backspace "
                 "刪字，按 ESC 取消。"));
#else
      loaderService->setPromptDescription(string(
          "\xE8\xAB\x8B\xE8\xBC\xB8\xE5\x85\xA5\xE6\x82\xA8\xE6\x83\xB3\xE6\x9F"
          "\xA5\xE8\xA9\xA2\xE5\x96\xAE\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "Enter "
          "\xE9\x96\x8B\xE5\xA7\x8B\xE6\x90\x9C\xE5\xB0\x8B\xE3\x80\x82\n\xE6"
          "\x8C\x89 Backspace \xE5\x88\xAA\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "ESC \xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"));
#endif
    } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
      loaderService->setPromptDescription(
          string("请输入您想查询单字，按 Enter 开始搜索。\n按 Backspace "
                 "删字，按 ESC 取消。"));
#else
      loaderService->setPromptDescription(string(
          "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE6\x82\xA8\xE6\x83\xB3\xE6\x9F"
          "\xA5\xE8\xAF\xA2\xE5\x8D\x95\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "Enter "
          "\xE5\xBC\x80\xE5\xA7\x8B\xE6\x90\x9C\xE5\xAF\xBB\xE3\x80\x82\n\xE6"
          "\x8C\x89 Backspace \xE5\x88\xA0\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "ESC \xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"));
#endif
    } else {
      loaderService->setPromptDescription(
          string("Pleae type the English word that you want to look up.") +
          string("\nPress Enter to perform search, press ESC to cancel"));
    }
  }
  if (m_searchType == 1) {
    if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
      loaderService->setPromptDescription(
          string("請輸入您想查詢單字，按 Enter 開始搜尋。\n按 Backspace "
                 "刪字，按 ESC 取消。"));
#else
      loaderService->setPromptDescription(string(
          "\xE8\xAB\x8B\xE8\xBC\xB8\xE5\x85\xA5\xE6\x82\xA8\xE6\x83\xB3\xE6\x9F"
          "\xA5\xE8\xA9\xA2\xE5\x96\xAE\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "Enter "
          "\xE9\x96\x8B\xE5\xA7\x8B\xE6\x90\x9C\xE5\xB0\x8B\xE3\x80\x82\n\xE6"
          "\x8C\x89 Backspace \xE5\x88\xAA\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "ESC \xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"));
#endif
    } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
      loaderService->setPromptDescription(
          string("请输入您想查询单字，按 Enter 开始搜索。\n按 Backspace "
                 "删字，按 ESC 取消。"));
#else
      loaderService->setPromptDescription(string(
          "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE6\x82\xA8\xE6\x83\xB3\xE6\x9F"
          "\xA5\xE8\xAF\xA2\xE5\x8D\x95\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "Enter "
          "\xE5\xBC\x80\xE5\xA7\x8B\xE6\x90\x9C\xE5\xAF\xBB\xE3\x80\x82\n\xE6"
          "\x8C\x89 Backspace \xE5\x88\xA0\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 "
          "ESC \xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"));
#endif
    } else {
      loaderService->setPromptDescription(
          string("Pleae type the Chinese word that you want to look up.") +
          string("\nPress Enter to perform search, press ESC to cancel"));
    }
  } else if (m_searchType == 2 || m_searchType == 4 || m_searchType == 5 ||
             m_searchType == 6) {
    string frontPrompt, auctionPrompt, mapPrompt, endPrompt;

    if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
      frontPrompt = "請輸入搜尋關鍵字";
      auctionPrompt = "請輸入商品關鍵字";
      mapPrompt = "請輸入地址或是地標";
      endPrompt = "，按 Enter 開始搜尋。\n按 Backspace 刪字，按 ESC 取消。";
#else
      frontPrompt =
          "\xe8\xab\x8b\xe8\xbc\xb8\xe5\x85\xa5\xe6\x90\x9c\xe5\xb0\x8b\xe9\x97"
          "\x9c\xe9\x8d\xb5\xe5\xad\x97";
      auctionPrompt =
          "\xe8\xab\x8b\xe8\xbc\xb8\xe5\x85\xa5\xe5\x95\x86\xe5\x93\x81\xe9\x97"
          "\x9c\xe9\x8d\xb5\xe5\xad\x97";
      mapPrompt =
          "\xe8\xab\x8b\xe8\xbc\xb8\xe5\x85\xa5\xe5\x9c\xb0\xe5\x9d\x80\xe6\x88"
          "\x96\xe6\x98\xaf\xe5\x9c\xb0\xe6\xa8\x99";
      endPrompt =
          "\xef\xbc\x8c\xe6\x8c\x89 Enter "
          "\xe9\x96\x8b\xe5\xa7\x8b\xe6\x90\x9c\xe5\xb0\x8b\xe3\x80\x82\n\xe6"
          "\x8c\x89 Backspace \xe5\x88\xaa\xe5\xad\x97\xef\xbc\x8c\xe6\x8c\x89 "
          "ESC \xe5\x8f\x96\xe6\xb6\x88\xe3\x80\x82";
#endif
    } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
      frontPrompt = "请输入搜索关键字";
      auctionPrompt = "请输入商品关键字";
      mapPrompt = "请输入地址或是地标";
      endPrompt = "，按 Enter 开始搜索。\n按 Backspace 删字，按 ESC 取消。";
#else
      frontPrompt =
          "\xe8\xaf\xb7\xe8\xbe\x93\xe5\x85\xa5\xe6\x90\x9c\xe7\xb4\xa2\xe5\x85"
          "\xb3\xe9\x94\xae\xe5\xad\x97";
      auctionPrompt =
          "\xe8\xaf\xb7\xe8\xbe\x93\xe5\x85\xa5\xe5\x95\x86\xe5\x93\x81\xe5\x85"
          "\xb3\xe9\x94\xae\xe5\xad\x97";
      mapPrompt =
          "\xe8\xaf\xb7\xe8\xbe\x93\xe5\x85\xa5\xe5\x9c\xb0\xe5\x9d\x80\xe6\x88"
          "\x96\xe6\x98\xaf\xe5\x9c\xb0\xe6\xa0\x87";
      endPrompt =
          "\xef\xbc\x8c\xe6\x8c\x89 Enter "
          "\xe5\xbc\x80\xe5\xa7\x8b\xe6\x90\x9c\xe7\xb4\xa2\xe3\x80\x82\n\xe6"
          "\x8c\x89 Backspace \xe5\x88\xa0\xe5\xad\x97\xef\xbc\x8c\xe6\x8c\x89 "
          "ESC \xe5\x8f\x96\xe6\xb6\x88\xe3\x80\x82";
#endif
    } else {
      frontPrompt = "Pleae type the English word that you want to look up.";
      auctionPrompt = "Please enter the item you're looking for.";
      mapPrompt = "Please enter the address or the name of the site.";
      endPrompt = "\nPress Enter to perform search, press ESC to cancel";
    }

    switch (m_searchType) {
      case 5:
        frontPrompt = auctionPrompt;
        break;
      case 6:
        frontPrompt = mapPrompt;
        break;
    }

    loaderService->setPromptDescription(frontPrompt + endPrompt);
  } else if (m_searchType == 3) {
    if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
      loaderService->setPromptDescription(
          string("請輸入股票名稱或是代號，按 Enter 後開啟瀏覽器搜尋。\n按 "
                 "Backspace 刪字，按 ESC 取消。"));
#else
      loaderService->setPromptDescription(string(
          "\xE8\xAB\x8B\xE8\xBC\xB8\xE5\x85\xA5\xE8\x82\xA1\xE7\xA5\xA8\xE5\x90"
          "\x8D\xE7\xA8\xB1\xE6\x88\x96\xE6\x98\xAF\xE4\xBB\xA3\xE8\x99\x9F\xEF"
          "\xBC\x8C\xE6\x8C\x89 Enter "
          "\xE5\xBE\x8C\xE9\x96\x8B\xE5\x95\x9F\xE7\x80\x8F\xE8\xA6\xBD\xE5\x99"
          "\xA8\xE6\x90\x9C\xE5\xB0\x8B\xE3\x80\x82\n\xE6\x8C\x89 Backspace "
          "\xE5\x88\xAA\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 ESC "
          "\xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"));
#endif
    } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
      loaderService->setPromptDescription(
          string("请输入股票名称或是代号，按 Enter 后开启浏览器搜索。\n按 "
                 "Backspace 删字，按 ESC 取消。"));
#else
      loaderService->setPromptDescription(string(
          "\xE8\xAF\xB7\xE8\xBE\x93\xE5\x85\xA5\xE8\x82\xA1\xE7\xA5\xA8\xE5\x90"
          "\x8D\xE7\xA7\xB0\xE6\x88\x96\xE6\x98\xAF\xE4\xBB\xA3\xE5\x8F\xB7\xEF"
          "\xBC\x8C\xE6\x8C\x89 Enter "
          "\xE5\x90\x8E\xE5\xBC\x80\xE5\x90\xAF\xE6\xB5\x8F\xE8\xA7\x88\xE5\x99"
          "\xA8\xE6\x90\x9C\xE5\xAF\xBB\xE3\x80\x82\n\xE6\x8C\x89 Backspace "
          "\xE5\x88\xA0\xE5\xAD\x97\xEF\xBC\x8C\xE6\x8C\x89 ESC "
          "\xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82\x22"));
#endif
    } else {
      loaderService->setPromptDescription(
          string("Please type the symbol or Chinese name of a stock.") +
          string("\nPress Enter to launch browser, press ESC to cancel"));
    }
  }
}

void OVAFSearchContext::showStopMessage(OVLoaderService* loaderService) {
  string locale = loaderService->locale();
  if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
    loaderService->notify("停止記錄");
#else
    loaderService->notify("\xE5\x81\x9C\xE6\xAD\xA2\xE8\xA8\x98\xE9\x8C\x84");
#endif
  } else if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
    loaderService->notify("停止记录");
#else
    loaderService->notify("\xE5\x81\x9C\xE6\xAD\xA2\xE8\xAE\xB0\xE5\xBD\x95");
#endif
  } else {
    loaderService->notify("Stop logging.");
  }
}

bool OVAFSearchContext::handleKey(OVKey* key, OVTextBuffer* readingText,
                                  OVTextBuffer* composingText,
                                  OVCandidateService* candidateService,
                                  OVLoaderService* loaderService) {
  if (composingText->composedText().length()) {
    return false;
  }
  if (readingText->composedText().length()) {
    return false;
  }

  if ((key->keyCode() == OVKeyCode::Left ||
       key->keyCode() == OVKeyCode::Right) &&
      m_logging) {
    if (composingText->isEmpty() && readingText->isEmpty()) return true;
  }

  string locale = loaderService->locale();

  if (key->keyCode() == m_module->m_cfgShortcutKey &&
      !key->isCombinedFunctionKey() && !m_logging) {
    OVOneDimensionalCandidatePanel* panel =
        candidateService->useVerticalCandidatePanel();
    OVCandidateList* list = panel->candidateList();
    list->clear();
    if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
      list->addCandidate("英漢字典");
      list->addCandidate("漢英字典");
      list->addCandidate("綜合搜尋");
      list->addCandidate("台灣股市");
      list->addCandidate("無名搜尋");
      list->addCandidate("拍賣搜尋");
      list->addCandidate("地圖搜尋");
#else
      list->addCandidate("\xe8\x8b\xb1\xe6\xbc\xa2\xe5\xad\x97\xe5\x85\xb8");
      list->addCandidate("\xe6\xbc\xa2\xe8\x8b\xb1\xe5\xad\x97\xe5\x85\xb8");
      list->addCandidate("\xe7\xb6\x9c\xe5\x90\x88\xe6\x90\x9c\xe5\xb0\x8b");
      list->addCandidate("\xe5\x8f\xb0\xe7\x81\xa3\xe8\x82\xa1\xe5\xb8\x82");
      list->addCandidate("\xe7\x84\xa1\xe5\x90\x8d\xe6\x90\x9c\xe5\xb0\x8b");
      list->addCandidate("\xe6\x8b\x8d\xe8\xb3\xa3\xe6\x90\x9c\xe5\xb0\x8b");
      list->addCandidate("\xe5\x9c\xb0\xe5\x9c\x96\xe6\x90\x9c\xe5\xb0\x8b");
#endif
    } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
      list->addCandidate("英汉字典");
      list->addCandidate("汉英字典");
      list->addCandidate("综合搜索");
      list->addCandidate("台湾股市");
      list->addCandidate("无名搜索");
      list->addCandidate("拍卖搜索");
      list->addCandidate("地图搜索");
#else
      list->addCandidate("\xe8\x8b\xb1\xe6\xb1\x89\xe5\xad\x97\xe5\x85\xb8");
      list->addCandidate("\xe6\xb1\x89\xe8\x8b\xb1\xe5\xad\x97\xe5\x85\xb8");
      list->addCandidate("\xe7\xbb\xbc\xe5\x90\x88\xe6\x90\x9c\xe7\xb4\xa2");
      list->addCandidate("\xe5\x8f\xb0\xe6\xb9\xbe\xe8\x82\xa1\xe5\xb8\x82");
      list->addCandidate("\xe6\x97\xa0\xe5\x90\x8d\xe6\x90\x9c\xe7\xb4\xa2");
      list->addCandidate("\xe6\x8b\x8d\xe5\x8d\x96\xe6\x90\x9c\xe7\xb4\xa2");
      list->addCandidate("\xe5\x9c\xb0\xe5\x9b\xbe\xe6\x90\x9c\xe7\xb4\xa2");
#endif
    } else {
      list->addCandidate("English-Chinese Dictionary");
      list->addCandidate("Chinese-English Dictionary");
      list->addCandidate("Web Search");
      list->addCandidate("Taiex (TW Stock)");
      list->addCandidate("Wretch.cc");
      list->addCandidate("Yahoo! Taiwan Auction");
      list->addCandidate("Yahoo! Taiwan Map");
    }
    composingText->clear();
    composingText->updateDisplay();
    readingText->clear();
    readingText->updateDisplay();
    loaderService->setLog("");

    panel->setCandidatesPerPage(7);
    panel->updateDisplay();
    panel->show();
    panel->yieldToCandidateEventHandler();
    m_searchString = "";
    return true;
  }

  if (m_logging) {
    if (key->keyCode() == OVKeyCode::Space) {
      if (m_searchString.size()) {
        m_searchString.append(" ");
        setLog(composingText, loaderService);
        return true;
      } else {
        return false;
      }
    }
    if ((key->keyCode() == m_module->m_cfgShortcutKey ||
         key->keyCode() == OVKeyCode::Esc) &&
        !key->isCombinedFunctionKey()) {
      showStopMessage(loaderService);
      loaderService->setPrompt("");
      loaderService->setPromptDescription("");
      loaderService->setLog("");
      m_searchString = "";
      m_logging = false;
      return true;
    }
    if (key->keyCode() == OVKeyCode::Backspace) {
      vector<string> codepoints =
          OVUTF8Helper::SplitStringByCodePoint(m_searchString);
      if (codepoints.size() <= 1) {
        m_searchString = "";
        setLog(composingText, loaderService);
        loaderService->beep();
        return true;
      }

      codepoints.pop_back();
      m_searchString = OVUTF8Helper::CombineCodePoints(codepoints);
      setLog(composingText, loaderService);
      return true;
    }

    if (key->keyCode() == OVKeyCode::Return ||
        key->keyCode() == OVKeyCode::Enter) {
      m_logging = false;
      if (m_searchString.length()) {
        if (!m_seachedText.size())
          m_seachedText.push_back(m_searchString);
        else if (m_searchString !=
                 string(m_seachedText.at(m_seachedText.size() - 1)))
          m_seachedText.push_back(m_searchString);

        if (m_seachedText.size() > 10)
          m_seachedText.erase(m_seachedText.begin(), m_seachedText.begin() + 1);
        m_searchedCursor = m_seachedText.size();

        if (m_searchType == 0 || m_searchType == 1) {
          loaderService->lookupInDefaultDictionary(m_searchString);
        } else if (m_searchType == 2) {
          loaderService->openURL(searchWeb(m_searchString, loaderService));
        } else if (m_searchType == 3) {
          loaderService->openURL(searchStock(m_searchString, loaderService));
        } else if (m_searchType == 4) {
          loaderService->openURL(searchWretch(m_searchString, loaderService));
        } else if (m_searchType == 5) {
          loaderService->openURL(searchAuction(m_searchString, loaderService));
        } else if (m_searchType == 6) {
          loaderService->openURL(searchMap(m_searchString, loaderService));
        }

        loaderService->setPrompt("");
        loaderService->setPromptDescription("");
        loaderService->setLog("");
        composingText->clear();
        composingText->updateDisplay();
        return true;
      } else {
        if (locale == "zh_TW" || locale == "zh-Hant") {
#ifndef _MSC_VER
          loaderService->notify("沒有輸入任何內容，取消搜尋");
#else
          loaderService->notify(
              "\xE6\xB2\x92\xE6\x9C\x89\xE8\xBC\xB8\xE5\x85\xA5\xE4\xBB\xBB\xE4"
              "\xBD\x95\xE5\x85\xA7\xE5\xAE\xB9\x2C\xE5\x8F\x96\xE6\xB6\x88\xE6"
              "\x90\x9C\xE5\xB0\x8B");
#endif
        } else if (locale == "zh_CN" || locale == "zh-Hans") {
#ifndef _MSC_VER
          loaderService->notify("没有输入任何内容，取消搜索");
#else
          loaderService->notify(
              "\xE6\xB2\xA1\xE6\x9C\x89\xE8\xBE\x93\xE5\x85\xA5\xE4\xBB\xBB\xE4"
              "\xBD\x95\xE5\x86\x85\xE5\xAE\xB9\xEF\xBC\x8C\xE5\x8F\x96\xE6\xB6"
              "\x88\xE6\x90\x9C\xE5\xAF\xBB");
#endif
        } else {
          loaderService->notify("Canceled, you did not enter any content.");
        }
        loaderService->setPrompt("");
        loaderService->setPromptDescription("");
        loaderService->setLog("");
        composingText->clear();
        composingText->updateDisplay();
        return true;
      }
    }
    if (key->keyCode() == OVKeyCode::Tab) return true;
    if (key->keyCode() == OVKeyCode::PageUp) return true;
    if (key->keyCode() == OVKeyCode::PageDown) return true;
    if (key->keyCode() == OVKeyCode::Home || key->keyCode() == OVKeyCode::End ||
        key->keyCode() == OVKeyCode::Up || key->keyCode() == OVKeyCode::Down) {
      if (!m_seachedText.size()) return true;
      if (key->keyCode() == OVKeyCode::Up) {
        if (m_searchedCursor == 0)
          m_searchedCursor = m_seachedText.size() - 1;
        else
          m_searchedCursor--;
      } else if (key->keyCode() == OVKeyCode::Down) {
        if (m_searchedCursor >= m_seachedText.size() - 1)
          m_searchedCursor = 0;
        else
          m_searchedCursor++;
      } else if (key->keyCode() == OVKeyCode::Home) {
        m_searchedCursor = 0;
      } else if (key->keyCode() == OVKeyCode::End) {
        m_searchedCursor = m_searchedCursor = m_seachedText.size() - 1;
      }

      if (m_searchedCursor >= m_seachedText.size()) return true;
      setLog(composingText, loaderService);
      m_searchString = m_seachedText.at(m_searchedCursor);
      loaderService->setLog(m_seachedText.at(m_searchedCursor));
      return true;
    }
    setLog(composingText, loaderService);
  }

  // English/Chinese Dictionary
  if (m_logging && m_searchType == 0) {
    char buf[2];
    if (key->isDirectTextKey() || key->isCapsLockOn())
      snprintf(buf, 2, "%c", toupper((char)key->keyCode()));
    else
      snprintf(buf, 2, "%c", (char)key->keyCode());
    m_searchString = m_searchString + string(buf);
    loaderService->setLog(m_searchString);
    return true;
  }

  return false;
}

bool OVAFSearchContext::handleDirectText(const string& text,
                                         OVTextBuffer* readingText,
                                         OVTextBuffer* composingText,
                                         OVCandidateService* candidateService,
                                         OVLoaderService* loaderService) {
  string locale = loaderService->locale();
  string composing = composingText->composedText();

  // If we are now in the logging mode, log the composingText to the internal
  // string. However, in the English-Chinese dictionary mode, we log the key
  // stroke but not the composingText.
  if (m_logging && m_searchType != 0) {
    m_searchString += text;

    composingText->clear();
    composingText->setText("");
    composingText->commit();
    composingText->setText(composing);
    composingText->updateDisplay();
    setLog(composingText, loaderService);

    return true;
  }

  // or pass thru
  composingText->setText(text);
  composingText->commit();
  composingText->setText(composing);
  composingText->updateDisplay();

  return true;
}

bool OVAFSearchContext::candidateNonPanelKeyReceived(
    OVCandidateService* candidateService, const OVKey* key,
    OVTextBuffer* readingText, OVTextBuffer* composingText,
    OVLoaderService* loaderService) {
  OVOneDimensionalCandidatePanel* panel =
      candidateService->useOneDimensionalCandidatePanel();
  // OVCandidateList* candidates = panel->candidateList();
  if (key->keyCode() == m_module->m_cfgShortcutKey) {
    panel->hide();
    panel->cancelEventHandler();
    loaderService->setPrompt("");
    loaderService->setPromptDescription("");
    loaderService->setLog("");
    composingText->clear();
    composingText->updateDisplay();
    m_logging = false;
    return true;
  }
  return false;
}

void OVAFSearchContext::candidateCanceled(OVCandidateService* candidateService,
                                          OVTextBuffer* readingText,
                                          OVTextBuffer* composingText,
                                          OVLoaderService* loaderService) {
  showStopMessage(loaderService);
  loaderService->setPrompt("");
  loaderService->setPromptDescription("");
  loaderService->setLog("");
  composingText->clear();
  composingText->updateDisplay();
  m_logging = false;
}

bool OVAFSearchContext::candidateSelected(OVCandidateService* candidateService,
                                          const string& text, size_t index,
                                          OVTextBuffer* readingText,
                                          OVTextBuffer* composingText,
                                          OVLoaderService* loaderService) {
  readingText->clear();
  readingText->updateDisplay();
  composingText->clear();
  composingText->updateDisplay();

  m_searchType = index;
  m_logging = true;

  setLog(composingText, loaderService);

  return true;
}

const string OVAFSearch::identifier() const { return OVAFSEARCH_IDENTIFIER; }

void OVAFSearch::loadConfig(OVKeyValueMap* moduleConfig,
                            OVLoaderService* loaderService) {
  // loaderService->logger("Search") << "loading config, shortcut key: " <<
  // moduleConfig->stringValueForKey("ShortcutKey") << endl;

  if (moduleConfig->hasKey("ShortcutKey")) {
    string v = moduleConfig->stringValueForKey("ShortcutKey");
    if (v.size()) m_cfgShortcutKey = v[0];
  }
}

void OVAFSearch::saveConfig(OVKeyValueMap* moduleConfig,
                            OVLoaderService* loaderService) {
  // loaderService->logger("Search") << "saving config" << endl;
  moduleConfig->setKeyStringValue("ShortcutKey", string(1, m_cfgShortcutKey));
}

};  // namespace OpenVanilla
