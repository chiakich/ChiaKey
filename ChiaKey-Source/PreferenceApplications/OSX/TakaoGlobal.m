/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/
// [AUTO_HEADER]

#import "TakaoGlobal.h"

#import "../../Frameworks/ChiakiSupport/Headers/ChiakiTextInputSource.h"
#import "TakaoHelper.h"

@interface TakaoGlobal (Private)
- (NSString *)_shortenedFilename:(NSString *)filename maxLength:(int)maxLength;
- (NSArray *)_enumeratePath:(NSString *)path withExtension:(NSString *)ext;
- (NSImage *)_smallIconForKeyboardLayout:(NSImage *)sourceImage;
@end

#pragma mark -

@implementation TakaoGlobal (Private)
- (NSString *)_shortenedFilename:(NSString *)filename maxLength:(int)maxLength {
  NSString *shortenedFilename = filename;
  if ([filename length] > (size_t)maxLength) {
    NSArray *pathComponents = [filename pathComponents];
    if ([pathComponents count] > 2) {
      shortenedFilename = [NSString
          stringWithFormat:@"%@%@/.../%@", [pathComponents objectAtIndex:0],
                           [pathComponents objectAtIndex:1],
                           [pathComponents
                               objectAtIndex:[pathComponents count] - 1]];
    }
  }
  return shortenedFilename;
}
- (NSArray *)_enumeratePath:(NSString *)path withExtension:(NSString *)ext {
  NSString *stdpath = [path stringByStandardizingPath];
  NSMutableArray *array = [NSMutableArray array];
  NSDirectoryEnumerator *direnum =
      [[NSFileManager defaultManager] enumeratorAtPath:stdpath];
  NSString *pname;
  while (pname = [direnum nextObject]) {
    if ([pname hasSuffix:ext])
      [array addObject:[stdpath stringByAppendingPathComponent:pname]];
    [direnum skipDescendants];
  }
  return array;
}
- (NSImage *)_smallIconForKeyboardLayout:(NSImage *)sourceImage {
  NSImage *image =
      [[[NSImage alloc] initWithSize:NSMakeSize(16, 16)] autorelease];
  [image lockFocus];
  [sourceImage drawInRect:NSMakeRect(0, 0, 16, 16)
                fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver
                 fraction:1.0];
  [image unlockFocus];
  return image;
}
@end

#pragma mark -

@implementation TakaoGlobal

- (void)dealloc {
  if (_sound) {
    if ([_sound isPlaying]) [_sound stop];
  }
  [_sound release];
  [_customSoundFilePath release];
  [_preferenceFilePath release];
  [_takaoDictionary release];
  [_keyboardLayoutIdentifierArray release];
  [_inputMethods release];
  [super dealloc];
}

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
- (void)setKeyboardLayoutUI {
  [_keyboardLayoutIdentifierArray release];
  _keyboardLayoutIdentifierArray = [[NSMutableArray alloc] init];
  NSString *keyboardLayout = [_takaoDictionary valueForKey:@"KeyboardLayout"];
  NSMenu *menu = [[[NSMenu alloc] initWithTitle:@"menu"] autorelease];
  CFArrayRef list = TISCreateInputSourceList(NULL, true);
  if (!list) {
    [_keyboardLayoutPopUpButton setMenu:menu];
    return;
  }

  int i = 0;
  int selectedIndex = -1;
  int indexOfEnUS = -1;
  for (i = 0; i < CFArrayGetCount(list); i++) {
    TISInputSourceRef source =
        (TISInputSourceRef)CFArrayGetValueAtIndex(list, i);

    CFStringRef type =
        TISGetInputSourceProperty(source, kTISPropertyInputSourceType);
    if (CFStringCompare(type, kTISTypeKeyboardLayout, kCFCompareEqualTo)) {
      continue;
    }

    CFURLRef iconURL = TISGetInputSourceProperty(source, kTISPropertyIconImageURL);
    NSImage *iconImage = nil;
    if (iconURL) {
      NSImage *sourceImage =
          [[[NSImage alloc] initWithContentsOfURL:(NSURL *)iconURL] autorelease];
      if (sourceImage) iconImage = [self _smallIconForKeyboardLayout:sourceImage];
    }

    CFStringRef sourceName =
        TISGetInputSourceProperty(source, kTISPropertyLocalizedName);
    NSString *sourceNameString = (NSString *)sourceName;

    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:sourceNameString
                                                      action:nil
                                               keyEquivalent:@""];
    if (iconImage) [menuItem setImage:iconImage];

    CFStringRef sourceId =
        TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
    NSString *sourceIdString = (NSString *)sourceId;
    int menuIndex = [_keyboardLayoutIdentifierArray count];
    if ([sourceIdString isEqualToString:@"com.apple.keylayout.US"])
      indexOfEnUS = menuIndex;

    if ([sourceIdString isEqualToString:keyboardLayout])
      selectedIndex = menuIndex;

    [_keyboardLayoutIdentifierArray addObject:sourceIdString];
    [menu addItem:menuItem];
  }
  [_keyboardLayoutPopUpButton setMenu:menu];

  if (selectedIndex < 0) {
    if (indexOfEnUS >= 0) {
      selectedIndex = indexOfEnUS;
      [_takaoDictionary setValue:@"com.apple.keylayout.US"
                          forKey:@"KeyboardLayout"];
    } else if ([_keyboardLayoutIdentifierArray count]) {
      selectedIndex = 0;
      [_takaoDictionary setValue:[_keyboardLayoutIdentifierArray objectAtIndex:0]
                          forKey:@"KeyboardLayout"];
    }
  }
  if (selectedIndex >= 0)
    [_keyboardLayoutPopUpButton selectItemAtIndex:selectedIndex];
  CFRelease(list);
}
#endif

- (void)setSoundUI {
  NSArray *aiffFiles = [self _enumeratePath:@"/System/Library/Sounds"
                              withExtension:@".aiff"];
  [_soundListPopUpButton removeAllItems];
  [_soundListPopUpButton addItemWithTitle:LFLSTR(@"Default")];
  NSEnumerator *e = [aiffFiles objectEnumerator];
  NSString *filePath;
  while (filePath = [e nextObject]) {
    [_soundListPopUpButton addItemWithTitle:[[filePath lastPathComponent]
                                                stringByDeletingPathExtension]];
  }
  NSString *soundFilePath = [_takaoDictionary valueForKey:@"SoundFilename"];
  if ([soundFilePath length]) {
    if ([soundFilePath isEqualToString:@"Default"]) {
      [_soundListPopUpButton selectItemAtIndex:0];
    } else {
      if ([soundFilePath hasPrefix:@"/System/Library/Sounds"]) {
        NSString *soundFilename =
            [[soundFilePath lastPathComponent] stringByDeletingPathExtension];
        [_soundListPopUpButton selectItemWithTitle:soundFilename];
      }
    }
    if (![_soundListPopUpButton selectedItem]) {
      if (![[NSFileManager defaultManager] fileExistsAtPath:soundFilePath]) {
        [_soundListPopUpButton selectItemAtIndex:0];
      } else {
        NSString *title = [NSString
            stringWithFormat:@"%@ (%@)", LFLSTR(@"Customized..."),
                             [self _shortenedFilename:[soundFilePath
                                                          lastPathComponent]
                                            maxLength:20]];
        [_soundListPopUpButton addItemWithTitle:title];
        [_soundListPopUpButton selectItem:[_soundListPopUpButton lastItem]];
        LFRetainAssign(_customSoundFilePath, soundFilePath);
      }
    } else {
      [_soundListPopUpButton addItemWithTitle:LFLSTR(@"Customized...")];
    }
  }
}

- (void)setUI {
  [_allowSecureInputCompositionCheckBox
      setTitle:LFLSTR(@"Compose in secure fields (no learning)")];

  if ([[_takaoDictionary valueForKey:@"ShouldPlaySoundOnTypingError"]
          isEqualToString:@"true"])
    [_soundCheckBox setIntValue:1];
  else
    [_soundCheckBox setIntValue:0];

  if ([[_takaoDictionary valueForKey:@"ShouldCheckUpdateOnLaunch"]
          isEqualToString:@"true"])
    [_useUpdateCheckBox setIntValue:1];
  else
    [_useUpdateCheckBox setIntValue:0];

  if ([[_takaoDictionary valueForKey:@"AllowSecureInputComposition"]
          isEqualToString:@"true"])
    [_allowSecureInputCompositionCheckBox setIntValue:1];
  else
    [_allowSecureInputCompositionCheckBox setIntValue:0];

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  if ([[_takaoDictionary valueForKey:@"ToggleInputMethodWithControlBackslash"]
          isEqualToString:@"true"])
    [_useCtrlBackSlashToggleInputMethod setIntValue:1];
  else
    [_useCtrlBackSlashToggleInputMethod setIntValue:0];
#else
  [_useCtrlBackSlashToggleInputMethod setHidden:YES];
//	[_useCtrlBackSlashToggleInputMethod setEnabled:NO];
//	NSString *title = [_useCtrlBackSlashToggleInputMethod title];
//	[_useCtrlBackSlashToggleInputMethod setTitle:[NSString
// stringWithFormat:@"%@ (10.5 only)", title]];
#endif

  if ([[_takaoDictionary valueForKey:@"OneDimensionalCandidatePanelStyle"]
          isEqualToString:@"horizontal"])
    [_candidateWindowStyleMatrix selectCellAtRow:0 column:1];
  else
    [_candidateWindowStyleMatrix selectCellAtRow:0 column:0];

  [self setSoundUI];
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  [self setKeyboardLayoutUI];
#endif
  [_highlightColorPopUpButton
      setColorSelection:[_takaoDictionary valueForKey:@"HighlightColor"]];
}

- (void)_addToModuleArrayWithIdentifer:(NSString *)identifer
                         localizedName:(NSString *)name {
  NSMutableDictionary *dictionary = [NSMutableDictionary
      dictionaryWithObjectsAndKeys:identifer, @"identifer", name, @"name", nil];
  NSArray *array = [_takaoDictionary objectForKey:@"ModulesSuppressedFromUI"];
  BOOL enabled = YES;

  NSEnumerator *enumerator = [array objectEnumerator];
  NSString *moduleName = nil;
  while (moduleName = [enumerator nextObject]) {
    if ([moduleName isEqualToString:identifer]) {
      enabled = NO;
      break;
    }
  }

  [dictionary setObject:[NSNumber numberWithBool:enabled] forKey:@"enabled"];
  [_inputMethods addObject:dictionary];
}
- (void)setInputMethods:(NSArray *)modules {
  [self awakeFromNib];

  if (!_inputMethods) {
    _inputMethods = [[NSMutableArray alloc] init];
  } else {
    [_inputMethods removeAllObjects];
  }
  [self _addToModuleArrayWithIdentifer:@"SmartMandarin"
                         localizedName:LFLSTR(@"Smart Phonetic")];
  [self _addToModuleArrayWithIdentifer:@"TraditionalMandarin"
                         localizedName:LFLSTR(@"Traditional Phonetic")];
  [self _addToModuleArrayWithIdentifer:@"Generic-cj-cin"
                         localizedName:LFLSTR(@"Cangjie")];
  [self _addToModuleArrayWithIdentifer:@"Generic-simplex-cin"
                         localizedName:LFLSTR(@"Simplex")];

  NSEnumerator *moduleEnumeartor = [modules objectEnumerator];
  NSArray *itemArray = nil;
  while (itemArray = [moduleEnumeartor nextObject]) {
    NSString *identifier = [itemArray objectAtIndex:0];
    NSString *localizedName = [itemArray objectAtIndex:1];
    [self _addToModuleArrayWithIdentifer:identifier
                           localizedName:localizedName];
  }

  [_moduleListTableView setDataSource:self];
  [_moduleListTableView setDelegate:(id)self];
  [_moduleListTableView reloadData];
}
- (void)awakeFromNib {
  if (_init) return;
  _init = YES;

  _takaoDictionary = [[NSMutableDictionary alloc] init];

  [_takaoDictionary setValue:@"true" forKey:@"ShouldPlaySoundOnTypingError"];
  [_takaoDictionary setValue:@"Default" forKey:@"SoundFilename"];
  [_takaoDictionary setValue:@"true" forKey:@"ShouldCheckUpdateOnLaunch"];
  [_takaoDictionary setValue:@"Default" forKey:@"HighlightColor"];
  [_takaoDictionary setValue:@"com.apple.keylayout.US"
                      forKey:@"KeyboardLayout"];
  [_takaoDictionary setValue:[NSArray array] forKey:@"ModulesSuppressedFromUI"];
  [_takaoDictionary setValue:@"vertical"
                      forKey:@"OneDimensionalCandidatePanelStyle"];
  [_takaoDictionary setValue:@"true"
                      forKey:@"ToggleInputMethodWithControlBackslash"];
  [_takaoDictionary setValue:@"false" forKey:@"AllowSecureInputComposition"];

  LFRetainAssign(_preferenceFilePath,
                 [TakaoHelper plistFilePath:PLIST_GLOBAL_FILENAME]);

  _sound = nil;
  _customSoundFilePath = nil;

  NSData *data = [NSData dataWithContentsOfFile:_preferenceFilePath
                                        options:0
                                          error:nil];
  if (data) {
    NSPropertyListFormat format;

    NSMutableDictionary *d = [NSPropertyListSerialization
        propertyListWithData:data
                      options:0
                       format:&format
                        error:nil];
    if (d) [_takaoDictionary addEntriesFromDictionary:d];
  }

  [self setUI];
  [self writePreference:self];
}

#pragma mark Write Preferece file

- (void)updateDictionary {
  if ([_soundCheckBox intValue])
    [_takaoDictionary setValue:@"true" forKey:@"ShouldPlaySoundOnTypingError"];
  else
    [_takaoDictionary setValue:@"false" forKey:@"ShouldPlaySoundOnTypingError"];

  if ([_useUpdateCheckBox intValue])
    [_takaoDictionary setValue:@"true" forKey:@"ShouldCheckUpdateOnLaunch"];
  else
    [_takaoDictionary setValue:@"false" forKey:@"ShouldCheckUpdateOnLaunch"];

  if ([_allowSecureInputCompositionCheckBox intValue])
    [_takaoDictionary setValue:@"true" forKey:@"AllowSecureInputComposition"];
  else
    [_takaoDictionary setValue:@"false" forKey:@"AllowSecureInputComposition"];

  if ([_useCtrlBackSlashToggleInputMethod intValue])
    [_takaoDictionary setValue:@"true"
                        forKey:@"ToggleInputMethodWithControlBackslash"];
  else
    [_takaoDictionary setValue:@"false"
                        forKey:@"ToggleInputMethodWithControlBackslash"];

  if ([_candidateWindowStyleMatrix selectedColumn] == 1)
    [_takaoDictionary setValue:@"horizontal"
                        forKey:@"OneDimensionalCandidatePanelStyle"];
  else
    [_takaoDictionary setValue:@"vertical"
                        forKey:@"OneDimensionalCandidatePanelStyle"];
}
- (void)doWrite {
  NSData *currentData = [NSData dataWithContentsOfFile:_preferenceFilePath
                                               options:0
                                                 error:nil];
  if (currentData) {
    NSPropertyListFormat format;

    NSMutableDictionary *d = [NSPropertyListSerialization
        propertyListWithData:currentData
                      options:0
                       format:&format
                        error:nil];
    if (d) {
      id ActivatedOutputFilters = [d valueForKey:@"ActivatedOutputFilters"];
      if (ActivatedOutputFilters)
        [_takaoDictionary setValue:ActivatedOutputFilters
                            forKey:@"ActivatedOutputFilters"];

      id ActivatedAroundFilters = [d valueForKey:@"ActivatedAroundFilters"];
      if (ActivatedAroundFilters)
        [_takaoDictionary setValue:ActivatedAroundFilters
                            forKey:@"ActivatedAroundFilters"];

      id PrimaryInputMethod = [d valueForKey:@"PrimaryInputMethod"];
      if (PrimaryInputMethod)
        [_takaoDictionary setValue:PrimaryInputMethod
                            forKey:@"PrimaryInputMethod"];
    }
  }

  NSData *data = [NSPropertyListSerialization
      dataWithPropertyList:_takaoDictionary
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];

  if (data) {
    [data writeToFile:_preferenceFilePath atomically:YES];
  }
}

#pragma mark Interface Builder actions

- (IBAction)writePreference:(id)sender {
  [self updateDictionary];
  [self doWrite];
}
- (IBAction)testSound:(id)sender {
  if (_sound) {
    if ([_sound isPlaying]) [_sound stop];
  }
  if ([[_soundListPopUpButton objectValue] intValue] == 0) {
    NSBeep();
    return;
  }
  if (_sound) {
    [_sound release];
    _sound = nil;
  }
  if ([[_soundListPopUpButton objectValue] intValue] ==
      ([[_soundListPopUpButton itemArray] count] - 1)) {
    if ([_customSoundFilePath length]) {
      _sound = [[NSSound alloc] initWithContentsOfFile:_customSoundFilePath
                                           byReference:YES];
    }
  } else {
    NSString *t = [_soundListPopUpButton titleOfSelectedItem];
    LFRetainAssign(_sound, [NSSound soundNamed:t]);
  }

  if (_sound) {
    [_sound setDelegate:(id)self];
    [_sound play];
    [_stopPlayiongButton setEnabled:YES];
  }
}
- (IBAction)changeSound:(id)sender {
  if (_sound) {
    if ([_sound isPlaying]) [_sound stop];
  }

  if ([[_soundListPopUpButton objectValue] intValue] == 0) {
    [_takaoDictionary setValue:@"Default" forKey:@"SoundFilename"];
    NSBeep();
    [self doWrite];
    return;
  }

  NSString *t = [sender titleOfSelectedItem];
  if ([[_soundListPopUpButton objectValue] intValue] ==
      ([[_soundListPopUpButton itemArray] count] - 1)) {
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    NSArray *filetypes = [NSArray
        arrayWithObjects:@"aiff", @"aif", @"aifc", @"wav", @"wave", @"snd",
                         @"au", @"mp3", @"ulw", @"mp4", @"m4a", nil];
    [openPanel setAllowsMultipleSelection:FALSE];
    [openPanel setCanCreateDirectories:NO];
    [openPanel
        setMessage:LFLSTR(
                       @"Please choose the audio clip which you want to use:")];
    [openPanel setTitle:LFLSTR(@"Choose Audio Clip...")];
    [openPanel setPrompt:LFLSTR(@"Choose")];
    [openPanel setAllowedFileTypes:filetypes];
    if ([openPanel runModal] == NSModalResponseOK) {
      NSString *soundFile = [[openPanel URL] path];
      if (![[NSFileManager defaultManager] fileExistsAtPath:soundFile]) {
        [_soundListPopUpButton selectItemAtIndex:0];
        _customSoundFilePath = nil;
      } else {
        NSString *title = [NSString
            stringWithFormat:@"%@ (%@)", LFLSTR(@"Customized..."),
                             [self _shortenedFilename:[soundFile
                                                          lastPathComponent]
                                            maxLength:20]];
        [[sender lastItem] setTitle:title];
        [_takaoDictionary setValue:soundFile forKey:@"SoundFilename"];
        _customSoundFilePath = [soundFile retain];
      }
    } else {
      [sender selectItemWithTitle:LFLSTR(@"Default")];
      [[sender lastItem] setTitle:LFLSTR(@"Customized...")];
      _customSoundFilePath = nil;
    }
    if ([_customSoundFilePath length])
      _sound = [[NSSound alloc] initWithContentsOfFile:_customSoundFilePath
                                           byReference:YES];
  } else {
    NSString *soundFilePath =
        [NSString stringWithFormat:@"/System/Library/Sounds/%@.aiff", t];
    [_takaoDictionary setValue:soundFilePath forKey:@"SoundFilename"];
    [[sender lastItem] setTitle:LFLSTR(@"Customized...")];
    _sound = [[NSSound soundNamed:t] retain];
  }

  [self doWrite];

  if (_sound) {
    [_sound setDelegate:(id)self];
    [_sound play];
    [_stopPlayiongButton setEnabled:YES];
  }
}
- (IBAction)stopSound:(id)sender {
  if (_sound) {
    if ([_sound isPlaying]) [_sound stop];
  }
  [_stopPlayiongButton setEnabled:NO];
}
- (IBAction)setKeyboardLayout:(id)sender {
  NSInteger selectedIndex = [sender indexOfSelectedItem];
  if (selectedIndex < 0 ||
      (NSUInteger)selectedIndex >= [_keyboardLayoutIdentifierArray count])
    return;

  NSString *keyboardLayout =
      [_keyboardLayoutIdentifierArray objectAtIndex:(NSUInteger)selectedIndex];
  if (!keyboardLayout) return;
  [_takaoDictionary setValue:keyboardLayout forKey:@"KeyboardLayout"];
  [self doWrite];
}
- (IBAction)resetKeyboardLayout:(id)sender {
  NSString *keyboardLayout = @"com.apple.keylayout.US";
  [_takaoDictionary setValue:keyboardLayout forKey:@"KeyboardLayout"];
  NSUInteger selectedIndex =
      [_keyboardLayoutIdentifierArray indexOfObject:keyboardLayout];
  if (selectedIndex != NSNotFound)
    [_keyboardLayoutPopUpButton selectItemAtIndex:selectedIndex];
  [self doWrite];
}
- (IBAction)setHighlightColor:(id)sender {
  NSString *colorString = [sender colorString];
  [_takaoDictionary setValue:colorString forKey:@"HighlightColor"];
  [self doWrite];
}

#pragma mark -
#pragma mark NSSound delegate

- (void)sound:(NSSound *)sound didFinishPlaying:(BOOL)finishedPlaying {
  [_stopPlayiongButton setEnabled:NO];
}

#pragma mark NSTableView datasource and delegate
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
#else
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
#endif

{
  return [_inputMethods count];
}

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(NSInteger)rowIndex
#else
- (id)tableView:(NSTableView *)aTableView
    objectValueForTableColumn:(NSTableColumn *)aTableColumn
                          row:(int)rowIndex
#endif

{
  NSDictionary *d = [_inputMethods objectAtIndex:rowIndex];
  if ([[aTableColumn identifier] isEqualToString:@"name"]) {
    return [d valueForKey:@"name"];
  } else if ([[aTableColumn identifier] isEqualToString:@"enabled"]) {
    return [d valueForKey:@"enabled"];
  }

  return nil;
}

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
- (void)tableView:(NSTableView *)aTableView
    setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn
               row:(NSInteger)rowIndex
#else
- (void)tableView:(NSTableView *)aTableView
    setObjectValue:(id)anObject
    forTableColumn:(NSTableColumn *)aTableColumn
               row:(int)rowIndex
#endif
{
  int suppressedCount = 0;
  if ([[aTableColumn identifier] isEqualToString:@"enabled"]) {
    NSDictionary *d = [_inputMethods objectAtIndex:rowIndex];
    [d setValue:anObject forKey:@"enabled"];
    NSEnumerator *enumerator = [_inputMethods objectEnumerator];
    while (d = [enumerator nextObject]) {
      if (![[d valueForKey:@"enabled"] boolValue]) {
        suppressedCount++;
      }
    }
    if (suppressedCount == [_inputMethods count]) {
      d = [_inputMethods objectAtIndex:rowIndex];
      [d setValue:[NSNumber numberWithBool:YES] forKey:@"enabled"];
      NSBeep();
    }
  }

  NSMutableArray *a = [NSMutableArray array];
  NSEnumerator *enumerator = [_inputMethods objectEnumerator];
  NSDictionary *d = nil;

  while (d = [enumerator nextObject]) {
    if (![[d valueForKey:@"enabled"] boolValue]) {
      [a addObject:[d valueForKey:@"identifer"]];
    }
  }

  [_takaoDictionary setValue:a forKey:@"ModulesSuppressedFromUI"];
  [self doWrite];
}

@end

#pragma mark -

@implementation TakaoColorPopUpButton

- (void)dealloc {
  [_colorMenu release];
  [_colors release];
  [_initString release];
  [_customizedColorMenuItem release];
  [super dealloc];
}

- (NSImage *)imageForColor:(NSColor *)aColor {
  NSImage *image =
      [[[NSImage alloc] initWithSize:NSMakeSize(30, 13)] autorelease];
  NSColor *fromColor = nil;
  NSColor *toColor = nil;

  //#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
  //	if ([aColor colorSpaceName] == NSDeviceWhiteColorSpace || [aColor
  // colorSpaceName] ==NSCalibratedWhiteColorSpace) { 		fromColor =
  // [NSColor colorWithCalibratedRed:1.0 green:1.0 blue:1.0 alpha:[aColor
  // alphaComponent]];
  //	}
  //	else if ([aColor colorSpaceName] == NSDeviceBlackColorSpace || [aColor
  // colorSpaceName] ==NSCalibratedBlackColorSpace) { 		fromColor =
  // [NSColor colorWithCalibratedRed:0.0 green:0.0 blue:0.0 alpha:[aColor
  // alphaComponent]];
  //	}
  //	else {
  //		fromColor = aColor;
  //	}
  //
  //	float hue = [fromColor hueComponent];
  //	float saturation = [fromColor saturationComponent];
  //	float brightness = [fromColor brightnessComponent] / 3.0;
  //	toColor = [NSColor colorWithCalibratedHue:hue saturation:saturation
  // brightness:brightness alpha:1.0]; #endif

  [image lockFocus];
  //#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
  //	NSGradient *g = [[[NSGradient alloc] initWithStartingColor:fromColor
  // endingColor:toColor] autorelease]; 	[g drawInRect:NSMakeRect(0, 0,
  // [image size].width, [image size].height) angle:270.0]; #else
  [aColor setFill];
  [NSBezierPath
      fillRect:NSMakeRect(0, 0, [image size].width, [image size].height)];
  //#endif
  [[NSColor blackColor] setStroke];
  [NSBezierPath
      strokeRect:NSMakeRect(0, 0, [image size].width, [image size].height)];
  [image unlockFocus];

  return image;
}

- (void)awakeFromNib {
  // this, because awakeFromNib's order is not guaranteed in Tiger, and if
  // setColorSelection is called prior (so awakeFromNib is called there if we
  // aren't yet properly "awaken"), we do this trick
  if (_colors) {
    return;
  }

  _colors = [[NSMutableArray alloc] init];
  _colorMenu = [[NSMenu alloc] initWithTitle:@"Colors"];

  NSMenuItem *purpleItem = [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Purple")
                                                       action:nil
                                                keyEquivalent:@""] autorelease];
  [purpleItem setImage:[self imageForColor:[NSColor purpleColor]]];
  NSMenuItem *greenItem = [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Green")
                                                      action:nil
                                               keyEquivalent:@""] autorelease];
  NSColor *greenColor = [NSColor colorWithCalibratedRed:0.23
                                                  green:0.68
                                                   blue:0.12
                                                  alpha:1.00];
  [greenItem setImage:[self imageForColor:greenColor]];
  NSMenuItem *yellowItem = [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Yellow")
                                                       action:nil
                                                keyEquivalent:@""] autorelease];
  [yellowItem setImage:[self imageForColor:[NSColor yellowColor]]];
  NSMenuItem *redItem = [[[NSMenuItem alloc] initWithTitle:LFLSTR(@"Red")
                                                    action:nil
                                             keyEquivalent:@""] autorelease];
  NSColor *redColor = [NSColor colorWithCalibratedRed:0.75
                                                green:0.00
                                                 blue:0.16
                                                alpha:1.00];
  [redItem setImage:[self imageForColor:redColor]];

  [_colorMenu addItem:purpleItem];
  [_colorMenu addItem:greenItem];
  [_colorMenu addItem:yellowItem];
  [_colorMenu addItem:redItem];
  [_colorMenu addItem:[NSMenuItem separatorItem]];

  [_colors addObject:@"Default"];
  [_colors addObject:@"Green"];
  [_colors addObject:@"Yellow"];
  [_colors addObject:@"Red"];
  [_colors addObject:@""];
  [_colors addObject:@"1.000000 1.000000 1.000000"];

  _customizedColorMenuItem = [[NSMenuItem alloc] init];
  [_customizedColorMenuItem setTitle:LFLSTR(@"Customized...")];
  [_customizedColorMenuItem setTarget:self];
  [_customizedColorMenuItem setImage:[self imageForColor:[NSColor whiteColor]]];
  [_customizedColorMenuItem setAction:@selector(customize:)];

  [_colorMenu addItem:_customizedColorMenuItem];
  [self setMenu:_colorMenu];

  if (_initString) {
    [self setColorSelection:_initString];
  }
}

- (IBAction)customize:(id)sender {
  [[NSColorPanel sharedColorPanel] orderFront:self];
  [[NSColorPanel sharedColorPanel] setTarget:self];
  [[NSColorPanel sharedColorPanel]
      setAction:@selector(changeCustomizedColorFromColorPicker:)];

  if (_controller) {
    [_controller setHighlightColor:self];
  }
}
- (void)changeCustomizedColorFromColorPicker:(id)sender {
  NSColor *color = [[NSColorPanel sharedColorPanel] color];
  [_customizedColorMenuItem setImage:[self imageForColor:color]];
  [self selectItem:_customizedColorMenuItem];

  NSString *newString = [NSString stringByColor:color];
  [_colors replaceObjectAtIndex:([_colors count] - 1) withObject:newString];

  if (_controller) {
    [_controller setHighlightColor:self];
  }
}
- (NSString *)colorString {
  NSInteger index = [self indexOfSelectedItem];
  return [_colors objectAtIndex:index];
}
- (void)setColorSelection:(NSString *)aString {
  if (![[self itemArray] count]) {
    [self awakeFromNib];
  }

  LFRetainAssign(_initString, aString);

  if ([_initString isEqualToString:@"Default"] ||
      [_initString isEqualToString:@"Purple"]) {
    [self selectItemAtIndex:0];
  } else if ([_initString isEqualToString:@"Green"]) {
    [self selectItemAtIndex:1];
  } else if ([_initString isEqualToString:@"Yellow"]) {
    [self selectItemAtIndex:2];
  } else if ([_initString isEqualToString:@"Red"]) {
    [self selectItemAtIndex:3];
  } else {
    NSColor *customColor = [_initString colorByString];
    if (!customColor) {
      [self selectItemAtIndex:0];
    } else {
      [self selectItem:_customizedColorMenuItem];
      [_customizedColorMenuItem setImage:[self imageForColor:customColor]];
      if (_colors) {
        [_colors replaceObjectAtIndex:([_colors count] - 1)
                           withObject:_initString];
      }
    }
  }
}

@end
