// [AUTO_HEADER]

#include "OVIMChewing.h"

namespace OpenVanilla {

bool ChewingFileExist(const char *path, const char *file,
                      const char *sep = "/") {
  string fn;
  fn = path;
  fn += sep;
  fn += file;

  struct stat st;
  if (stat(fn.c_str(), &st)) return false;
  return true;
}

bool ChewingCheckData(const char *path) {
  char *files[8] = {"ch_index.dat", "dict.dat", "fonetree.dat", "ph_index.dat",
                    "pinyin.tab",   "swkb.dat", "symbols.dat",  "us_freq.dat"};

  for (int i = 0; i < 8; i++)
    if (!ChewingFileExist(path, files[i])) return false;
  return true;
}

CHEWING_API char *chewing_buffer_String_old(ChewingContext *chewingContext,
                                            int from, int to) {
  int i;
  char *s = (char *)calloc(1 + chewingContext->output->chiSymbolBufLen,
                           sizeof(char) * MAX_UTF8_SIZE);
  if (from >= 0 && to < chewingContext->output->chiSymbolBufLen) {
    for (i = from; i <= to; i++) {
      strcat(s, (char *)(chewingContext->output->chiSymbolBuf[i].s));
    }
  }
  return s;
}

CHEWING_API char *chewing_buffer_String_End_old(ChewingContext *chewingContext,
                                                int from) {
  return chewing_buffer_String_old(chewingContext, from,
                                   chewingContext->output->chiSymbolBufLen - 1);
}

void OVIMChewingContext::startSession(OVLoaderService *loaderService) {
  chewing_handle_Enter(im);
}

void OVIMChewingContext::endSession(OVLoaderService *loaderService) {
  chewing_handle_Enter(im);
}

bool OVIMChewingContext::handleKey(OVKey *key, OVTextBuffer *readingText,
                                   OVTextBuffer *composingText,
                                   OVCandidateService *candidateService,
                                   OVLoaderService *loaderService) {
  OVOneDimensionalCandidatePanel *panel =
      candidateService->useOneDimensionalCandidatePanel();

  if (!panel->isVisible() && composingText->isEmpty() &&
      key->isShiftPressed() && key->keyCode() == OVKeyCode::Space) {
    composingText->appendText("　");
    composingText->commit();
    return true;
  }

  // see if it's the combination of CTRL-OPT-x
  if (!(key->isOptPressed() && key->isCtrlPressed())) {
    if (key->isCommandPressed()) return false;

    if (!isprint(key->keyCode()) && composingText->isEmpty()) return false;
  }

  KeyPress(key, composingText, candidateService, loaderService);
  if (chewing_keystroke_CheckIgnore(im)) return false;

  CandidateWindow(candidateService, loaderService);
  Redraw(composingText, loaderService);
  Notify(loaderService);
  return true;
}

void OVIMChewingContext::KeyPress(OVKey *key, OVTextBuffer *composingText,
                                  OVCandidateService *candidateService,
                                  OVLoaderService *loaderService) {
  int k = key->keyCode();
  Capslock(key);

  if (k == OVKeyCode::Space) {
    key->isShiftPressed() ? chewing_handle_ShiftSpace(im)
                          : chewing_handle_Space(im);
  } else if (k == OVKeyCode::Left) {
    key->isShiftPressed() ? chewing_handle_ShiftLeft(im)
                          : chewing_handle_Left(im);
  } else if (k == OVKeyCode::Right) {
    key->isShiftPressed() ? chewing_handle_ShiftRight(im)
                          : chewing_handle_Right(im);
  } else if (k == OVKeyCode::Down) {
    chewing_handle_Down(im);
  } else if (k == OVKeyCode::Up) {
    chewing_handle_Up(im);
  } else if (k == OVKeyCode::PageUp) {
    chewing_handle_PageUp(im);
  } else if (k == OVKeyCode::PageDown) {
    chewing_handle_PageDown(im);
  } else if (k == OVKeyCode::Esc) {
    chewing_handle_Esc(im);
  } else if (k == OVKeyCode::Tab) {
    chewing_handle_Tab(im);
  } else if (k == OVKeyCode::Home) {
    chewing_handle_Home(im);
  } else if (k == OVKeyCode::End) {
    chewing_handle_End(im);
  } else if (k == OVKeyCode::Delete) {
    chewing_handle_Del(im);
  } else if (k == OVKeyCode::Backspace) {
    chewing_handle_Backspace(im);
  } else if (k == OVKeyCode::Return) {
    chewing_handle_Enter(im);
  } else {
    DefaultKey(key);
  }
}
void OVIMChewingContext::DefaultKey(OVKey *key) {
  if (key->isCtrlPressed()) {
    if ((key->keyCode() >= '0') && (key->keyCode() <= '9')) {
      chewing_handle_CtrlNum(im, (key->keyCode()));
    } else if (key->isOptPressed()) {
      chewing_handle_CtrlOption(im, (key->keyCode()));
    }
    return;
  }
  chewing_handle_Default(im, (key->isShiftPressed()) ? toupper(key->keyCode())
                                                     : tolower(key->keyCode()));
}

void OVIMChewingContext::Capslock(OVKey *key) {
  if (key->isCapsLockOn()) {
    if (chewing_get_ChiEngMode(im) == CHINESE_MODE) {
      chewing_handle_Capslock(im);
    }
  } else if (chewing_get_ChiEngMode(im) != CHINESE_MODE) {
    chewing_handle_Capslock(im);
  }
}

void OVIMChewingContext::CandidateWindow(OVCandidateService *candidateService,
                                         OVLoaderService *loaderService) {
  // void CandidateWindow(OVCandidate *textbar, OVService *srv) {
  OVOneDimensionalCandidatePanel *panel =
      candidateService->useOneDimensionalCandidatePanel();
  OVCandidateList *candidates = panel->candidateList();
  if (chewing_cand_TotalPage(im) > 0) {
    char s[64];
    int i = 1;
    char *cand_string;

    candidates->clear();
    chewing_cand_Enumerate(im);
    string selectKeys;
    while (chewing_cand_hasNext(im)) {
      if (i > chewing_cand_ChoicePerPage(im)) break;
      char c = im->data->config.selKey[i - 1];
      selectKeys += c;
      cand_string = chewing_cand_String(im);
      candidates->addCandidate(cand_string);
      free(cand_string);
      i++;
    }

    panel->setCandidateKeys(selectKeys, loaderService);
    // panel->candidatesPerPage();
    // sprintf(s," %d/%d", chewing_cand_CurrentPage(im) + 1,
    // chewing_cand_TotalPage(im));
    panel->show();
  } else {
    panel->hide();
  }
}
void OVIMChewingContext::Notify(OVLoaderService *loaderService) {
  if (chewing_aux_Length(im) != 0) {
    loaderService->notify(chewing_aux_String(im));
    loaderService->beep();
  }
}
void OVIMChewingContext::Redraw(OVTextBuffer *composingText,
                                OVLoaderService *loaderService) {
  if (chewing_commit_Check(im)) {
    const char *s = chewing_commit_String(im);
    composingText->clear();
    composingText->appendText(s);
    composingText->commit();
  } else {
    composingText->clear();
  }

  int ps = -1, pe = -1;

  int ips = im->output->PointStart;
  int ipe = im->output->PointEnd;

  if (ips > -1 && ipe != 0) {
    if (ipe > 0) {
      ps = ips;
      pe = ps + ipe;
    } else {
      ps = ips + ipe;
      pe = ips;
    }
  }

  const char *s1, *s2, *s3;
  int zuin_count;

  int pos = (int)chewing_cursor_Current(im);

  s1 = chewing_buffer_String_old(im, 0, pos - 1);
  composingText->appendText(s1);

  s2 = chewing_zuin_String(im, &zuin_count);
  composingText->appendText(s2);

  s3 = chewing_buffer_String_End_old(im, pos);
  composingText->appendText(s3);
  composingText->setCursorPosition(pos);
  composingText->setHighlightMark(OVTextBuffer::RangePair(ps, pe - ps + 1));
}
const string OVIMChewing::localizedName(const string &locale) {
  if (locale == "zh_TW" || locale == "zh-hant") {
    return "酷音";
  } else if (locale == "zh_CN" || locale == "zh-hans") {
    return "酷音";
  } else {
    return "Chewing";
  }
}

bool OVIMChewing::initialize(OVPathInfo *pathInfo,
                             OVLoaderService *loaderService) {
  ChewingConfigData config;

  string hashdir = pathInfo->writablePath + OVPathHelper::Separator();
  string chewingpath = pathInfo->resourcePath + OVPathHelper::Separator();
  loaderService->logger("OVIMChewing") << chewingpath << endl;
  loaderService->logger("OVIMChewing") << hashdir << endl;
  if (!ChewingCheckData(chewingpath.c_str())) {
    return false;
  }

  chewing_Init("/usr/share/chewing/", "/tmp/a b/");
  chewingContext = chewing_new();
  loaderService->logger("init ok") << chewingpath << endl;
  return true;
}

void OVIMChewing::finalize() {}

void OVIMChewing::loadConfig(OVKeyValueMap *moduleConfig,
                             OVLoaderService *loaderService) {
  ChewingConfigData config;
  int kb =
      atoi(moduleConfig->stringValueForKeyWithDefault("KeyboardLayout", "0")
               .c_str());
  chewing_set_KBType(chewingContext, kb);

  const char *defaultSelectionKeys = "1234567890";

  if (kb == KB_HSU) defaultSelectionKeys = "asdfghjkl0";
  if (kb == KB_DVORAK_HSU) defaultSelectionKeys = "aoeuhtn7890";

  for (int i = 0; i < MAX_SELKEY; i++)
    config.selKey[i] = defaultSelectionKeys[i];

  // SpaceChewingp-specific settings
  config.bSpaceAsSelection = 1;
  config.candPerPage = atoi(
      moduleConfig->stringValueForKeyWithDefault("candPerPage", "7").c_str());
  config.maxChiSymbolLen = 20;
  config.bAddPhraseForward = 0;
  int direction =
      atoi(moduleConfig->stringValueForKeyWithDefault("addPhraseForward", "0")
               .c_str());
  config.bAddPhraseForward = direction > 0 ? 1 : 0;
  chewing_Configure(chewingContext, &config);
}

};  // namespace OpenVanilla
