// [AUTO_HEADER]

#import "OpenVanillaController.h"

#import <AudioToolbox/AudioToolbox.h>
#import <Carbon/Carbon.h>
#import <string.h>

#import "CVApplicationController.h"
#import "CVNotifyController.h"
#import "NSStringExtension.h"

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
#import "CVKeyboardHelper.h"
#endif

#import "LFUtilities.h"

static OpenVanillaController *OVCActiveContext = nil;
static id OVCActiveContextSender = nil;

static BOOL OVCInvokeBooleanSelector(id object, SEL selector) {
  if (![object respondsToSelector:selector]) {
    return NO;
  }

  NSMethodSignature *signature = [object methodSignatureForSelector:selector];
  if (!signature || [signature numberOfArguments] != 2) {
    return NO;
  }

  const char *returnType = [signature methodReturnType];
  if (strcmp(returnType, @encode(BOOL)) != 0 &&
      strcmp(returnType, @encode(bool)) != 0 &&
      strcmp(returnType, @encode(char)) != 0 &&
      strcmp(returnType, @encode(unsigned char)) != 0) {
    return NO;
  }

  BOOL result = NO;
  @try {
    NSInvocation *invocation =
        [NSInvocation invocationWithMethodSignature:signature];
    [invocation setTarget:object];
    [invocation setSelector:selector];
    [invocation invoke];
    [invocation getReturnValue:&result];
  } @catch (NSException *exception) {
    return NO;
  }

  return result;
}

static BOOL OVCClientReportsSecureInput(id client) {
  SEL selectors[] = {
      @selector(isSecureTextEntry),
      @selector(secureTextEntry),
      @selector(isSecure),
      @selector(secure),
  };

  for (size_t index = 0; index < sizeof(selectors) / sizeof(selectors[0]);
       ++index) {
    if (OVCInvokeBooleanSelector(client, selectors[index])) {
      return YES;
    }
  }

  return NO;
}

static BOOL OVCIsSecureInputActive(id client) {
  return IsSecureEventInputEnabled() || OVCClientReportsSecureInput(client);
}

static BOOL OVCAllowsSecureInputComposition() {
  OVKeyValueMap kvm = [OpenVanillaLoader sharedLoader]->configKeyValueMap();
  return kvm.stringValueForKey("AllowSecureInputComposition") == "true";
}

class OVCSecureInputModeScope {
 public:
  OVCSecureInputModeScope(PVLoaderService *loaderService, bool enabled)
      : m_loaderService(loaderService) {
    m_loaderService->setSecureInputMode(enabled);
  }

  ~OVCSecureInputModeScope() { m_loaderService->setSecureInputMode(false); }

 private:
  PVLoaderService *m_loaderService;
};

@implementation OpenVanillaController
- (void)dealloc {
  if (OVCActiveContext == self) {
    [OpenVanillaController setActiveContext:nil sender:nil];
  }

  // delete C++ objects here
  delete _context;
  [_composingBuffer release];
  [super dealloc];
}

- (id)initWithServer:(IMKServer *)server
            delegate:(id)delegate
              client:(id)inputClient {
  if (self = [super initWithServer:server
                          delegate:delegate
                            client:inputClient]) {
    // NSLog(@"New controller (delegate %08x, client %08x)", delegate,
    // inputClient);

    _doNotClearContextStateEvenWithForcedCommit = NO;
    _updateCommitStringBeforeCommit = NO;
    _commitFromOurselves = NO;
    _composingBuffer = [NSMutableString new];

    [[OpenVanillaLoader sharedLock] lock];
    _context = [OpenVanillaLoader sharedLoader]->createContext();
    [[OpenVanillaLoader sharedLock] unlock];
  }

  return self;
}

+ (void)setActiveContext:(OpenVanillaController *)context sender:(id)sender {
  OVCActiveContext = context;

  id tmp = OVCActiveContextSender;
  OVCActiveContextSender = [sender retain];
  [tmp release];
}

#pragma mark Send string to client

+ (void)sendComposedStringToCurrentlyActiveContext:(NSString *)text {
  if (OVCActiveContext && OVCActiveContextSender) {
    // NSLog(@"sending direct text: %@", text);
    [OVCActiveContext sendComposedStringToClient:text
                                          sender:OVCActiveContextSender];
  } else {
    // NSLog(@"send text: no active context found");
  }
}
- (void)sendComposedStringToClient:(NSString *)text sender:(id)sender {
  [_composingBuffer setString:text];

  // same as deactivate, but we can't hide the dictionary panel
  // (because this is exactly called by dictionary itself)

  // force commit
  _commitFromOurselves = YES;
  [self commitComposition:sender];

  _context->clear();
  _context->deactivate();
  [[[NSApp delegate] verticalCandidateController]
      updateContent:_context->candidateService()->accessVerticalCandidatePanel()
            atPoint:NSMakePoint(0., 0.)];
  [[[NSApp delegate] horizontalCandidateController]
      updateContent:_context->candidateService()
                        ->accessHorizontalCandidatePanel()
            atPoint:NSMakePoint(0., 0.)];

  _context->candidateService()->accessVerticalCandidatePanel()->finishUpdate();
  _context->candidateService()
      ->accessHorizontalCandidatePanel()
      ->finishUpdate();

  [[[NSApp delegate] tooltipController] hide];
  [[[NSApp delegate] searchController] hide];

  PVLoaderService *loaderService = [OpenVanillaLoader sharedLoaderService];
  loaderService->setPrompt("");
  loaderService->setPromptDescription("");
  loaderService->setLog("");
  _context->activate();
}

#pragma mark Fix cursor position

// To fix the curson position in some applications such as WOW
- (BOOL)_fixCursorPosition:(NSPoint)cursorPosition {
  NSRect frame = [[NSScreen mainScreen] frame];
  BOOL hasFocus = NO;

  NSArray *screens = [NSScreen screens];
  NSEnumerator *enumerator = [screens objectEnumerator];
  NSScreen *screen;
  while (screen = [enumerator nextObject]) {
    NSRect screenFrame = [screen frame];

    if (!hasFocus && cursorPosition.x >= NSMinX(screenFrame) &&
        cursorPosition.x <= NSMaxX(screenFrame) &&
        cursorPosition.y >= NSMinY(screenFrame) &&
        cursorPosition.y <= NSMaxY(screenFrame)) {
      frame = screenFrame;
      hasFocus = YES;
      break;
    }
  }

  if (hasFocus) {
    return YES;
  }

  return NO;
}

#pragma mark -
#pragma mark Input Method Kit methods

- (void)_updateClient:(id)client
       cursorPosition:(int)position
         segmentPairs:
             (const PVCombinedUTF16TextBuffer::SegmentPairVector &)pairs {
  NSMutableAttributedString *attrString = [[[NSMutableAttributedString alloc]
      initWithString:_composingBuffer
          attributes:[NSDictionary dictionary]] autorelease];

  // selectionRange means "cursor position"
  NSRange selectionRange = NSMakeRange(position, 0);

  // NSLog(@"marker");
  for (PVCombinedUTF16TextBuffer::SegmentPairVector::const_iterator iter =
           pairs.begin();
       iter != pairs.end(); ++iter) {
    OVTextBuffer::RangePair range = (*iter).first;
    PVCombinedUTF16TextBuffer::SegmentType type = (*iter).second;

    NSRange markerRange = NSMakeRange(range.first, range.second);
    int markerStyle;

    switch (type) {
      case PVCombinedUTF16TextBuffer::Highlight:
        markerStyle = NSUnderlineStyleThick;
        break;
      default:
        markerStyle = NSUnderlineStyleSingle;
        break;
    }

    int sectionNumber = (int)(iter - pairs.begin());

    // NSLog(@"segment %d (%d, %d), type %d", sectionNumber, range.first,
    // range.second,  type);

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
    NSDictionary *attrDict = [NSDictionary
        dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:markerStyle],
                                     NSUnderlineStyleAttributeName,
                                     [NSNumber numberWithInt:sectionNumber],
                                     NSMarkedClauseSegmentAttributeName, nil];
#else
    NSDictionary *attrDict = [NSDictionary
        dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:markerStyle],
                                     @"UnderlineStyleAttribute",
                                     [NSNumber numberWithInt:sectionNumber],
                                     @"MarkedClauseSegmentAttribute", nil];
#endif

    [attrString setAttributes:attrDict range:markerRange];
  }

  [client setMarkedText:attrString
         selectionRange:selectionRange
       replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
  // NSLog(@"updating string: %@", attrString);
}

- (NSUInteger)recognizedEvents:(id)sender {
  //	NSLog(@"recognizedEvents (client %08x)", sender);
  return NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged |
         NSEventMaskMouseEntered | NSEventMaskLeftMouseDown |
         NSEventMaskLeftMouseDragged;
}
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
- (NSString *)currentKeyboardLayout {
  PVPlistValue *dict = [OpenVanillaLoader sharedLoader]->configRootDictionary();
  if (dict) {
    PVPlistValue *layoutValue = dict->valueForKey("KeyboardLayout");
    if (layoutValue) {
      string layoutString = layoutValue->stringValue();
      NSString *layout = [NSString stringWithUTF8String:layoutString.c_str()];
      if ([[CVKeyboardHelper sharedSendKey] validateKeyboardLayout:layout]) {
        return layout;
      }
    }
  }
  return @"com.apple.keylayout.US";
}
#endif

- (void)activateServer:(id)sender {
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  if ([[sender bundleIdentifier] isEqualToString:@"com.apple.Terminal"]) {
    // NSLog(@"applying app-specific fix for Terminal.app");
    _doNotClearContextStateEvenWithForcedCommit = YES;
  }
#endif

  if ([[sender bundleIdentifier] isEqualToString:@"com.microsoft.Powerpoint"]) {
    // NSLog(@"applying app-specific fix for PowerPoint (2008)");
    _updateCommitStringBeforeCommit = YES;
  }

  [[OpenVanillaLoader sharedInstance] syncUserCannedMessages];

  [OpenVanillaLoader sharedLoader]->syncLoaderConfig();
  OVKeyValueMap kvm = [OpenVanillaLoader sharedLoader]->configKeyValueMap();
  string style = kvm.stringValueForKey("OneDimensionalCandidatePanelStyle");
  if (OVWildcard::Match(style, "horizontal")) {
    _context->candidateService()->setOneDimensionalPanelVertical(false);
  } else {
    _context->candidateService()->setOneDimensionalPanelVertical(true);
  }

  if (kvm.hasKey("CandidateTextFontHeight")) {
    float fontHeight =
        atof(kvm.stringValueForKey("CandidateTextFontHeight").c_str());
    if (fontHeight < 16.0 || fontHeight > 96.0) {
      fontHeight = 18.0;
    }

    [[[NSApp delegate] verticalCandidateController]
        setCandidateTextHeight:fontHeight];
    [[[NSApp delegate] horizontalCandidateController]
        setCandidateTextHeight:fontHeight];
  }

  // NSLog(@"activateServer (client %08x)", sender);
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  // NSLog(@"current keyboard: %@", [self currentKeyboardLayout]);
  [sender overrideKeyboardWithKeyboardNamed:[self currentKeyboardLayout]];
#else
  [sender overrideKeyboardWithKeyboardNamed:@"com.apple.keylayout.US"];
#endif
  [_composingBuffer setString:@""];

  _context->activate();

  [[[NSApp delegate] symbolController] restoreWindowStatus];
  [OpenVanillaController setActiveContext:self sender:sender];
}
- (void)deactivateServer:(id)sender {
  // NSLog(@"deactivateServer (client %08x), identifier: %@", sender, [sender
  // bundleIdentifier]);

  [OpenVanillaController setActiveContext:nil sender:nil];

  // force commit
  _commitFromOurselves = YES;
  [self commitComposition:sender];

  _context->clear();
  _context->deactivate();

  [[[NSApp delegate] verticalCandidateController]
      updateContent:_context->candidateService()->accessVerticalCandidatePanel()
            atPoint:NSMakePoint(0., 0.)];
  [[[NSApp delegate] horizontalCandidateController]
      updateContent:_context->candidateService()
                        ->accessHorizontalCandidatePanel()
            atPoint:NSMakePoint(0., 0.)];

  _context->candidateService()->accessVerticalCandidatePanel()->finishUpdate();
  _context->candidateService()
      ->accessHorizontalCandidatePanel()
      ->finishUpdate();

  // NSLog(@"deactivateServer (client %08x)", sender);

  [[[NSApp delegate] symbolController] temporaryHide];
  [[[NSApp delegate] tooltipController] hide];
  [[[NSApp delegate] searchController] hide];
  PVLoaderService *loaderService = [OpenVanillaLoader sharedLoaderService];
  loaderService->setPrompt("");
  loaderService->setPromptDescription("");
  loaderService->setLog("");
}
- (void)commitComposition:(id)sender {
  // NSLog(@"commitComposition (client %08x), identifier: %@", sender, [sender
  // bundleIdentifier]);

  bool readingBufferEmpty = _context->readingText()->isEmpty();

  if (!_commitFromOurselves) {
    if (_doNotClearContextStateEvenWithForcedCommit) {
      //			[_composingBuffer setString:@""];
    } else {
      string residue = _context->residueComposingTextBeforeDeactivation();
      [_composingBuffer
          setString:[NSString stringWithUTF8String:residue.c_str()]];
      // NSLog(@"residue: %@", [NSString stringWithUTF8String:residue.c_str()]);
    }
  }

  if ([_composingBuffer length]) {
    if (_updateCommitStringBeforeCommit) {
      PVCombinedUTF16TextBuffer::SegmentPairVector emptyPairs;
      [self _updateClient:sender
           cursorPosition:[_composingBuffer length]
             segmentPairs:emptyPairs];
    }

    // NSLog(@"buffer has something: %@", _composingBuffer);

    // instead of commit what's remaining on compsing buffer, we ask for
    // residues
    [sender insertText:_composingBuffer
        replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
    [_composingBuffer setString:@""];
  } else {
    if (!readingBufferEmpty && !_commitFromOurselves) {
      PVCombinedUTF16TextBuffer::SegmentPairVector emptySegs;
      [self _updateClient:sender cursorPosition:0 segmentPairs:emptySegs];
    }

    // NSLog(@"buffer is empty");
  }

  if (!_commitFromOurselves) {
    // NSLog(@"not commit from ourselves, clear context state");

    if (!_doNotClearContextStateEvenWithForcedCommit) _context->clear();
  } else {
    // NSLog(@"commit from ourselves");
    _commitFromOurselves = NO;
  }
  // NSLog(@"commitComposition end");
}
- (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
  // update loader service
  PVLoaderService *loaderService = [OpenVanillaLoader sharedLoaderService];

  // NSLog(@"handleEvent (client %08x), type = %08x, modifier = %08x, event:
  // %@", sender, [event type], [event modifierFlags], event);

  if ([event type] == NSEventTypeFlagsChanged) {
    // handles caps lock and shift here
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
    if (!([event modifierFlags] & NSEventModifierFlagControl)) {
      id appDelegate = [NSApp delegate];
      NSWindow *window =
          [[appDelegate inputMethodToggleWindowController] window];
      if ([window isVisible]) {
        [window orderOut:self];
      }
    }
#endif
  } else if ([event type] == NSEventTypeKeyDown) {
    BOOL secureInputActive = OVCIsSecureInputActive(sender);
    BOOL allowSecureInputComposition = OVCAllowsSecureInputComposition();
    BOOL secureInputComposition =
        secureInputActive && allowSecureInputComposition;

    if (secureInputActive && !allowSecureInputComposition) {
      [_composingBuffer setString:@""];
      _context->clear();
      [self _resetUI];
      return NO;
    }

    OVCSecureInputModeScope secureInputModeScope(loaderService,
                                                secureInputComposition);
    bool isHandled = false;

    NSString *chars = [event characters];
    NSEventModifierFlags cocoaModifiers = [event modifierFlags];
    unsigned short virtualKeyCode = [event keyCode];
    unsigned int vanillaModifiers = 0;

    if (cocoaModifiers & NSEventModifierFlagCapsLock)
      vanillaModifiers |= OVKeyMask::CapsLock;
    // if (cocoaModifiers & NSNumericPadKeyMask) vanillaModifiers |=
    // OVKeyMask::NumLock;
    if (cocoaModifiers & NSEventModifierFlagShift)
      vanillaModifiers |= OVKeyMask::Shift;
    if (cocoaModifiers & NSEventModifierFlagControl)
      vanillaModifiers |= OVKeyMask::Ctrl;
    if (cocoaModifiers & NSEventModifierFlagOption)
      vanillaModifiers |= OVKeyMask::Opt;
    if (cocoaModifiers & NSEventModifierFlagCommand)
      vanillaModifiers |= OVKeyMask::Command;

    UInt32 numKeys[16] = {
        // 0,1,2,3,4,5
        // 6,7,8,9,.,+,-,*,/,=
        0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5b, 0x5c, 0x41, 0x45, 0x4e, 0x43, 0x4b, 0x51};

    for (size_t i = 0; i < 16; i++) {
      if (virtualKeyCode == numKeys[i]) {
        vanillaModifiers |=
            OVKeyMask::NumLock;  // only if it's numpad key we put mask back
        break;
      }
    }

    PVKeyImpl *keyImpl;

    bool isPrintable = false;
    UniChar unicharCode = 0;
    if ([chars length] > 0) {
      unicharCode = [chars characterAtIndex:0];

      // translates CTRL-[A-Z] to the correct PVKeyImpl
      if (cocoaModifiers & NSEventModifierFlagControl) {
        if (unicharCode < 27) {
          unicharCode += ('a' - 1);
        } else {
          switch (unicharCode) {
            case 27:
              unicharCode =
                  (cocoaModifiers & NSEventModifierFlagShift) ? '{' : '[';
              break;
            case 28:
              unicharCode =
                  (cocoaModifiers & NSEventModifierFlagShift) ? '|' : '\\';
              break;
            case 29:
              unicharCode =
                  (cocoaModifiers & NSEventModifierFlagShift) ? '}' : ']';
              break;
            case 31:
              unicharCode =
                  (cocoaModifiers & NSEventModifierFlagShift) ? '_' : '-';
              break;
          }
        }
      }

      UniChar remappedNSEventCode = unicharCode;

      // remap; fix 10.6 "bug"
      switch (unicharCode) {
        case NSUpArrowFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::Up;
          break;
        case NSDownArrowFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::Down;
          break;
        case NSLeftArrowFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::Left;
          break;
        case NSRightArrowFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::Right;
          break;
        case NSDeleteFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::Delete;
          break;
        case NSHomeFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::Home;
          break;
        case NSEndFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::End;
          break;
        case NSPageUpFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::PageUp;
          break;
        case NSPageDownFunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::PageDown;
          break;
        case NSF1FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F1;
          break;
        case NSF2FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F2;
          break;
        case NSF3FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F3;
          break;
        case NSF4FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F4;
          break;
        case NSF5FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F5;
          break;
        case NSF6FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F6;
          break;
        case NSF7FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F7;
          break;
        case NSF8FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F8;
          break;
        case NSF9FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F9;
          break;
        case NSF10FunctionKey:
          remappedNSEventCode = (UniChar)OVKeyCode::F10;
          break;
      }

      unicharCode = remappedNSEventCode;

      if (unicharCode < 128) {
        //                if (!isprint((char)unicharCode) && (cocoaModifiers &
        //                NSShiftKeyMask)) vanillaModifiers |= OVKeyMask::Shift;
        keyImpl = new PVKeyImpl(unicharCode, vanillaModifiers);
      } else {
        keyImpl = new PVKeyImpl(string([chars UTF8String]), vanillaModifiers);
        isPrintable = true;
      }
    } else {
      //		    if (cocoaModifiers & NSShiftKeyMask)
      // vanillaModifiers |= OVKeyMask::Shift;
      keyImpl = new PVKeyImpl(0, vanillaModifiers);
    }

    OVKey key(keyImpl);

    id appDelegate = [NSApp delegate];

    // Backslash + Ctrl
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
    if (key.isCtrlPressed() && key.keyCode() == '\\') {
      OVKeyValueMap kvm = [OpenVanillaLoader sharedLoader]->configKeyValueMap();
      string shouldUseControlBackslahs =
          kvm.stringValueForKey("ToggleInputMethodWithControlBackslash");
      if (shouldUseControlBackslahs == "true") {
        [_composingBuffer setString:[NSString string]];
        [self commitComposition:sender];
        [self _resetUI];

        NSRect lineHeightRect;
        NSDictionary *attribute =
            [sender attributesForCharacterIndex:0
                            lineHeightRectangle:&lineHeightRect];
        NSPoint cursorPosition = lineHeightRect.origin;

        [[appDelegate inputMethodToggleWindowController]
            useScreenOfPoint:cursorPosition];
        [[appDelegate inputMethodToggleWindowController] moveToNextInputMethod];
        return YES;
      }
    }
#endif

    // NSLog(@"hanlding event, code = %d (%d or %x), modifiers = %x, vkeycode =
    // %x", unicharCode, (char)unicharCode, (char)unicharCode, vanillaModifiers,
    // virtualKeyCode);

    isHandled = _context->handleKeyEvent(&key);

    if (_context->composingText()->isCommitted()) {
      [_composingBuffer
          setString:[NSString stringWithUTF8String:_context->composingText()
                                                       ->composedCommittedText()
                                                       .c_str()]];
      _commitFromOurselves = YES;
      [self commitComposition:sender];
      _context->composingText()->finishCommit();
    }

    PVCombinedUTF16TextBuffer combinedBuffer(
        *(_context->composingText()),
        *(_context->readingText()) /* , true, true */);
    string promptText = loaderService->prompt();
    PVTextBuffer promptBuffer;

    if (promptText.size()) {
      string logText = loaderService->log();
      promptBuffer.setText(logText);
      promptBuffer.setCursorPosition(
          OVUTF8Helper::SplitStringByCodePoint(logText).size());
      promptBuffer.updateDisplay();

      PVTextBuffer newBuffer;
      newBuffer.setText(combinedBuffer.composedText());
      newBuffer.setCursorPosition(combinedBuffer.cursorPosition());
      newBuffer.updateDisplay();
      combinedBuffer = PVCombinedUTF16TextBuffer(promptBuffer, newBuffer);
      [_composingBuffer setString:@" "];
    } else {
      [_composingBuffer
          setString:[NSString stringWithUTF8String:combinedBuffer.composedText()
                                                       .c_str()]];
    }

    size_t cursorIndex = 0;
    cursorIndex = combinedBuffer.wideCursorPosition();

    // obtain the cursor position; note the cursor index can't be composing
    // buffer's length this part needs to be very careful, outbound index causes
    // crash

    NSRect lineHeightRect;
    NSPoint cursorPosition;
    float fontHeight;

    if (promptText.size()) {
      NSDictionary *attribute =
          [sender attributesForCharacterIndex:0
                          lineHeightRectangle:&lineHeightRect];
      cursorPosition = lineHeightRect.origin;
      //			cursorPosition = [self
      //_fixCursorPosition:cursorPosition];
      float bufferHeight = 0;
      NSFont *currentFont = [attribute objectForKey:NSFontAttributeName];
      if (currentFont != nil) {
        bufferHeight = [currentFont pointSize];
      } else {
        bufferHeight = lineHeightRect.size.height;
      }
      [[appDelegate searchController] setBufferHeight:bufferHeight];
      string promptDescription = loaderService->promptDescription();
      string buffer = combinedBuffer.composedText();
      cursorIndex = combinedBuffer.wideCursorPosition();
      if ([self _fixCursorPosition:cursorPosition])
        [[appDelegate searchController]
               showWithPrompt:[NSString stringWithUTF8String:promptText.c_str()]
            promptDescription:[NSString stringWithUTF8String:promptDescription
                                                                 .c_str()]
                       buffer:[NSString stringWithUTF8String:buffer.c_str()]
                        point:cursorPosition
                  readingFrom:(int)promptBuffer.cursorPosition()
                readingLength:(int)_context->composingText()->codePointCount()
                  cursorIndex:cursorIndex];
      else
        [[appDelegate searchController]
               showWithPrompt:[NSString stringWithUTF8String:promptText.c_str()]
            promptDescription:[NSString stringWithUTF8String:promptDescription
                                                                 .c_str()]
                       buffer:[NSString stringWithUTF8String:buffer.c_str()]
                  readingFrom:(int)promptBuffer.cursorPosition()
                readingLength:(int)_context->composingText()->codePointCount()
                  cursorIndex:cursorIndex];

      cursorPosition = [[appDelegate searchController] cursorPosition];
      fontHeight = [[appDelegate searchController] bufferHeight];
    } else {
      [[appDelegate searchController] hide];
      if (_context->composingText()->shouldUpdate() ||
          _context->readingText()->shouldUpdate()) {
        if (cursorIndex && cursorIndex >= [_composingBuffer length])
          cursorIndex = [_composingBuffer length];
        [self _updateClient:sender
             cursorPosition:cursorIndex
               segmentPairs:combinedBuffer.wideSegmentPairs()];
        _context->composingText()->finishUpdate();
        _context->readingText()->finishUpdate();
      }
      cursorIndex = combinedBuffer.wideCursorPosition();
      if (cursorIndex && cursorIndex >= [_composingBuffer length]) {
        if ([_composingBuffer length])
          cursorIndex = [_composingBuffer length] - 1;
        else
          cursorIndex = 0;
      }
      [sender attributesForCharacterIndex:cursorIndex
                      lineHeightRectangle:&lineHeightRect];
      cursorPosition = lineHeightRect.origin;
      fontHeight = lineHeightRect.size.height;
    }

    // NSLog(@"cursorPosition: %f, %f", cursorPosition.x, cursorPosition.y);

    // update candiates
    PVHorizontalCandidatePanel *horizontalPanel =
        _context->candidateService()->accessHorizontalCandidatePanel();
    PVVerticalCandidatePanel *verticalPanel =
        _context->candidateService()->accessVerticalCandidatePanel();
    PVOneDimensionalCandidatePanel *oneDimensionalPanel = verticalPanel;
    OVCandidatePanel *lastUsedPanel =
        _context->candidateService()->lastUsedPanel();

    if (lastUsedPanel == horizontalPanel || lastUsedPanel == verticalPanel) {
      if (lastUsedPanel == verticalPanel) {
        oneDimensionalPanel = verticalPanel;
        [[appDelegate verticalCandidateController] setFontHeight:fontHeight];
        [[appDelegate verticalCandidateController]
            updateContent:verticalPanel
                  atPoint:cursorPosition];
        oneDimensionalPanel->finishUpdate();
      } else {
        oneDimensionalPanel = horizontalPanel;
        [[appDelegate horizontalCandidateController] setFontHeight:fontHeight];
        [[appDelegate horizontalCandidateController]
            updateContent:horizontalPanel
                  atPoint:cursorPosition];
        oneDimensionalPanel->finishUpdate();
      }
    }

    PVPlainTextCandidatePanel *plainTextPanel =
        _context->candidateService()->accessPlainTextCandidatePanel();
    if (lastUsedPanel == plainTextPanel) {
      [[appDelegate plainTextCandidateController] updateContent:plainTextPanel
                                                        atPoint:cursorPosition];
      plainTextPanel->finishUpdate();
    }

    if ((!oneDimensionalPanel->isVisible() && !plainTextPanel->isVisible()) &&
        _context->composingText()->toolTipText().size()) {
      [[appDelegate tooltipController]
          showToolTip:[NSString stringWithUTF8String:_context->composingText()
                                                         ->toolTipText()
                                                         .c_str()]
              atPoint:cursorPosition];
      _context->composingText()->clearToolTip();
    } else {
      [[appDelegate tooltipController] hide];
    }

    if (loaderService->shouldBeep()) {
      OVKeyValueMap kvm = [OpenVanillaLoader sharedLoader]->configKeyValueMap();
      string shouldPlaySound =
          kvm.stringValueForKey("ShouldPlaySoundOnTypingError");
      string soundFilename = kvm.stringValueForKey("SoundFilename");
      if (shouldPlaySound == "true") {
        if (!soundFilename.size() || soundFilename == "Default") {
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
          AudioServicesPlayAlertSound(kUserPreferredAlert);
#else
          NSBeep();
#endif
        } else {
          NSSound *userSound = [[[NSSound alloc]
              initWithContentsOfFile:[NSString
                                         stringWithUTF8String:soundFilename
                                                                  .c_str()]
                         byReference:YES] autorelease];
          if (userSound) [userSound play];
        }
      }
    }

    if (!secureInputComposition && loaderService->notifyMessage().size()) {
      vector<string> messages = loaderService->notifyMessage();
      for (vector<string>::iterator iter = messages.begin();
           iter != messages.end(); ++iter) {
        string notifymessage = *iter;
        NSString *messgae =
            [NSString stringWithUTF8String:notifymessage.c_str()];
        [CVNotifyController notify:messgae];
      }
    }
    if (!secureInputComposition && loaderService->loaderFeatureKey().length()) {
      string key = loaderService->loaderFeatureKey();
      string value = loaderService->loaderFeatureValue();

      if (key == "LaunchApp") {
        NSString *applicationPath =
            [NSString stringWithUTF8String:value.c_str()];
        if ([applicationPath length]) {
          [[NSWorkspace sharedWorkspace] openFile:applicationPath];
        }
      }
    }

    if (!secureInputComposition && loaderService->URLToOpen().size()) {
      [[NSWorkspace sharedWorkspace]
          openURL:[NSURL
                      URLWithString:[NSString
                                        stringWithUTF8String:loaderService
                                                                 ->URLToOpen()
                                                                 .c_str()]]];
    }

    string savedPrompt = loaderService->prompt();
    string savedPromptDescription = loaderService->promptDescription();
    string savedLog = loaderService->log();
    loaderService->resetState();
    loaderService->setPrompt(savedPrompt);
    loaderService->setPromptDescription(savedPromptDescription);
    loaderService->setLog(savedLog);

    return isHandled;
  }

  return NO;
}

#pragma mark -
#pragma mark Input Menu actions

- (void)_resetUI {
  PVLoaderService *loaderService = [OpenVanillaLoader sharedLoaderService];
  loaderService->resetState();
  [[[NSApp delegate] verticalCandidateController] hide];
  [[[NSApp delegate] horizontalCandidateController] hide];
  //	[[[NSApp delegate] plainTextCandidateController] hideTextWindow];
  [[[NSApp delegate] tooltipController] hide];
  [[[NSApp delegate] searchController] hide];
}
- (void)switchInputMethodAction:(id)sender {
  NSMenuItem *menuItem = [sender objectForKey:@"IMKCommandMenuItem"];
  [OpenVanillaLoader sharedLoader]->setPrimaryInputMethod(
      [[menuItem representedObject] UTF8String]);
  [OpenVanillaLoader sharedLoader]->syncSandwichConfig();

  [self _resetUI];
  NSString *msg =
      [NSString stringWithFormat:@"%@%@", LFLSTR(@"Current Input Method:"),
                                 [menuItem title]];
  [CVNotifyController notify:msg];
}
- (void)toggleOutputFilterAction:(id)sender {
  NSMenuItem *menuItem = [sender objectForKey:@"IMKCommandMenuItem"];
  string identifier = [[menuItem representedObject] UTF8String];
  [OpenVanillaLoader sharedLoader]->toggleOutputFilter(identifier);
  [self _resetUI];

  NSString *msg;
  if ([OpenVanillaLoader sharedLoader]->isOutputFilterActivated(identifier)) {
    msg = [NSString
        stringWithFormat:@"%@%@", LFLSTR(@"Enable "), [menuItem title]];
  } else {
    msg = [NSString
        stringWithFormat:@"%@%@", LFLSTR(@"Disable "), [menuItem title]];
  }
  [CVNotifyController notify:msg];
}
- (void)toggleAroundFilterAction:(id)sender {
  NSMenuItem *menuItem = [sender objectForKey:@"IMKCommandMenuItem"];
  string identifier = [[menuItem representedObject] UTF8String];
  [OpenVanillaLoader sharedLoader]->toggleAroundFilter(identifier);
  [self _resetUI];

  NSString *msg;
  if ([OpenVanillaLoader sharedLoader]->isAroundFilterActivated(identifier)) {
    msg = [NSString
        stringWithFormat:@"%@%@", LFLSTR(@"Enable "), [menuItem title]];
  } else {
    msg = [NSString
        stringWithFormat:@"%@%@", LFLSTR(@"Disable "), [menuItem title]];
  }
  [CVNotifyController notify:msg];
}
- (void)symbolAction:(id)sender {
  if ([[[[NSApp delegate] symbolController] window] isVisible]) {
    [[[NSApp delegate] symbolController] hide:self];
  } else {
    [[[NSApp delegate] symbolController] show:self];
  }
}
- (void)helpAction:(id)sender {
  NSString *urlString = @"https://github.com/akira02/ChiaKey";
  NSURL *url = [NSURL URLWithString:urlString];
  [[NSWorkspace sharedWorkspace] openURL:url];
  [self _resetUI];
}

- (void)preferenceAction:(id)sender {
  NSString *sharedSupprtPath = [[NSBundle mainBundle] sharedSupportPath];

#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
  NSString *preferencePath =
      [sharedSupprtPath stringByAppendingPathComponent:@"Preferences.app"];
#else
  NSString *preferencePath =
      [sharedSupprtPath stringByAppendingPathComponent:@"PreferencesTiger.app"];
#endif

  NSURL *preferenceURL = [NSURL fileURLWithPath:preferencePath];
  if (![[NSWorkspace sharedWorkspace] openURL:preferenceURL]) {
    if (@available(macOS 10.15, *)) {
      NSURL *applicationURL = [[NSWorkspace sharedWorkspace]
          URLForApplicationWithBundleIdentifier:
              @"com.chiakey.inputmethod.ChiaKey.Preferences"];
      if (applicationURL) {
        [[NSWorkspace sharedWorkspace]
            openApplicationAtURL:applicationURL
                    configuration:[NSWorkspaceOpenConfiguration configuration]
                completionHandler:nil];
      }
    }
  }
  [self _resetUI];
}

- (void)aboutAction:(id)sender {
  [[NSApp delegate] showAboutWindow:sender];
  [self _resetUI];
}

#pragma mark The Input Menu

- (NSMenuItem *)_createMenuItemWithIndentifer:(string)identifier
                                localizedName:(NSString *)localizedName
                                keyEquivalent:(NSString *)key {
  NSMenuItem *menuItem = [[[NSMenuItem alloc] init] autorelease];
  if ([OpenVanillaLoader sharedLoader]->isFailedModule(identifier)) {
    [menuItem setEnabled:NO];
  } else {
    if ([OpenVanillaLoader sharedLoader]->primaryInputMethod() == identifier)
      [menuItem setState:NSControlStateValueOn];

    [menuItem setTarget:self];
    [menuItem setAction:@selector(switchInputMethodAction:)];
    [menuItem
        setRepresentedObject:[NSString
                                 stringWithUTF8String:identifier.c_str()]];
  }
  [menuItem setTitle:localizedName];
  if (key && [key length] > 0) {
    [menuItem setKeyEquivalent:key];
    [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand |
                                           NSEventModifierFlagControl];
  }
  return menuItem;
}

- (NSMenu *)menu {
  NSMenu *menu = [[[NSMenu alloc] init] autorelease];
  NSArray *a = [[NSApp delegate] inputMethodsArray];
  NSEnumerator *e = [a objectEnumerator];
  NSDictionary *d = nil;
  while (d = [e nextObject]) {
    NSString *identifier = [d valueForKey:@"identifier"];
    NSString *localizedName = [d valueForKey:@"localizedName"];
    [menu
        addItem:[self _createMenuItemWithIndentifer:string(
                                                        [identifier UTF8String])
                                      localizedName:localizedName
                                      keyEquivalent:nil]];
  }

  vector<pair<string, string> >::iterator iter;
  vector<pair<string, string> > idNamePairs;

  [menu addItem:[NSMenuItem separatorItem]];

  idNamePairs =
      [OpenVanillaLoader sharedLoader]->allAroundFilterIdentifiersAndNames();
  for (iter = idNamePairs.begin(); iter != idNamePairs.end(); ++iter) {
    pair<string, string> idNamePair = *iter;
    string identifier = idNamePair.first;
    string localizedName = idNamePair.second;

    if (OVWildcard::Match(identifier, "ReverseLookup-*")) {
      continue;
    }

    if (identifier == "Evaluator") {
      if (![OpenVanillaLoader sharedLoader]->isAroundFilterActivated(
              identifier)) {
        [OpenVanillaLoader sharedLoader]->toggleAroundFilter(identifier);
      }
      continue;
    }
    NSMenuItem *menuItem = [[[NSMenuItem alloc] init] autorelease];

    if ([OpenVanillaLoader sharedLoader]->isFailedModule(identifier)) {
      [menuItem setEnabled:NO];
    } else {
      if ([OpenVanillaLoader sharedLoader]->isAroundFilterActivated(identifier))
        [menuItem setState:NSControlStateValueOn];

      [menuItem setTarget:self];
      [menuItem setAction:@selector(toggleAroundFilterAction:)];
      [menuItem
          setRepresentedObject:[NSString
                                   stringWithUTF8String:identifier.c_str()]];
    }
    [menuItem setTitle:[NSString stringWithUTF8String:localizedName.c_str()]];
    [menu addItem:menuItem];
  }

  [menu addItem:[NSMenuItem separatorItem]];

  idNamePairs =
      [OpenVanillaLoader sharedLoader]->allOutputFilterIdentifiersAndNames();
  for (iter = idNamePairs.begin(); iter != idNamePairs.end(); ++iter) {
    pair<string, string> idNamePair = *iter;
    string identifier = idNamePair.first;
    string localizedName = idNamePair.second;

    if (identifier == "OVOFHanConvert-SC2TC") continue;

    NSMenuItem *menuItem = [[[NSMenuItem alloc] init] autorelease];

    if ([OpenVanillaLoader sharedLoader]->isFailedModule(identifier))
      [menuItem setEnabled:NO];

    if ([OpenVanillaLoader sharedLoader]->isOutputFilterActivated(identifier))
      [menuItem setState:NSControlStateValueOn];

    if (identifier == "OVOFFullWidthCharacter") {
      [menuItem setKeyEquivalent:@" "];
      [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand |
                                             NSEventModifierFlagShift];
    }

    if (identifier == "OVOFHanConvert-TC2SC") {
      [menuItem setKeyEquivalent:@"g"];
      [menuItem
          setKeyEquivalentModifierMask:NSEventModifierFlagCommand |
                                       NSEventModifierFlagControl];
    }

    [menuItem setTarget:self];
    [menuItem setAction:@selector(toggleOutputFilterAction:)];
    [menuItem
        setRepresentedObject:[NSString
                                 stringWithUTF8String:identifier.c_str()]];
    [menuItem setTitle:[NSString stringWithUTF8String:localizedName.c_str()]];
    [menu addItem:menuItem];
  }

  [menu addItem:[NSMenuItem separatorItem]];

  NSMenuItem *symbolMenuItem = [[[NSMenuItem alloc] init] autorelease];
  [symbolMenuItem setTarget:self];
  [symbolMenuItem setAction:@selector(symbolAction:)];
  [symbolMenuItem setTitle:LFLSTR(@"Symbols")];
  [symbolMenuItem setKeyEquivalent:@"."];
  [symbolMenuItem
      setKeyEquivalentModifierMask:NSEventModifierFlagCommand |
                                   NSEventModifierFlagControl];
  [menu addItem:symbolMenuItem];

  NSMenuItem *helpMenuItem = [[[NSMenuItem alloc] init] autorelease];
  [helpMenuItem setTarget:self];
  [helpMenuItem setAction:@selector(helpAction:)];
  [helpMenuItem setTitle:LFLSTR(@"Help")];
  [menu addItem:helpMenuItem];

  NSMenuItem *prefMenuItem = [[[NSMenuItem alloc] init] autorelease];
  [prefMenuItem setTarget:self];
  [prefMenuItem setAction:@selector(preferenceAction:)];
  [prefMenuItem setTitle:LFLSTR(@"Preferences...")];
  [menu addItem:prefMenuItem];

  NSMenuItem *aboutMenuItem = [[[NSMenuItem alloc] init] autorelease];
  [aboutMenuItem setTarget:self];
  [aboutMenuItem setAction:@selector(aboutAction:)];
  [aboutMenuItem setTitle:LFLSTR(@"About ChiaKey")];
  [menu addItem:aboutMenuItem];

  return menu;
}
@end
