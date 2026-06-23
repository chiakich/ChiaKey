//
// LFCrossDevelopmentTools.m
//
// Copyright (c) 2007-2010 Lithoglyph Inc. All rights reserved.
//

#import "LFCrossDevelopmentTools.h"

typedef struct {
  SInt32 OSXVersionMajor;
  SInt32 OSXVersionMinor;
} LFCrossDevelopmentInfo;

static LFCrossDevelopmentInfo *LFSharedCrossDevelopmentInfo = NULL;

LFCrossDevelopmentInfo *LFGetSharedCrossDevelopmentInfo() {
  if (!LFSharedCrossDevelopmentInfo) {
    LFSharedCrossDevelopmentInfo =
        (LFCrossDevelopmentInfo *)calloc(1, sizeof(LFCrossDevelopmentInfo));

    Gestalt(gestaltSystemVersionMajor,
            &LFSharedCrossDevelopmentInfo->OSXVersionMajor);
    Gestalt(gestaltSystemVersionMinor,
            &LFSharedCrossDevelopmentInfo->OSXVersionMinor);
  }

  return LFSharedCrossDevelopmentInfo;
}

// mutually exclusive things
BOOL LFIsRunningUnderOSX10_5AndAbove() {
  LFCrossDevelopmentInfo *info = LFGetSharedCrossDevelopmentInfo();
  return info->OSXVersionMajor == 10 && info->OSXVersionMinor >= 5;
}

BOOL LFIsRunningUnderOSX10_4Only() {
  LFCrossDevelopmentInfo *info = LFGetSharedCrossDevelopmentInfo();
  return info->OSXVersionMajor == 10 && info->OSXVersionMinor == 4;
}
