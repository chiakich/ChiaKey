// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

@interface CVSendKey : NSObject {
}
+ (CVSendKey *)sharedSendKey;
- (void)typeString:(NSString *)string;
@end

enum {
  /* modifier keys */
  kVirtualCapsLockKey = 0x039,
  kVirtualShiftKey = 0x038,
  kVirtualControlKey = 0x03B,
  kVirtualOptionKey = 0x03A,
  kVirtualCommandKey = 0x037,
  /* editing/utility keys */
  kVirtualHelpKey = 0x072,
  kVirtualDeleteKey = 0x033,
  kVirtualTabKey = 0x030,
  kVirtualEnterKey = 0x04C,
  kVirtualReturnKey = 0x024,
  kVirtualEscapeKey = 0x035,
  kVirtualForwardDeleteKey = 0x075,
  /* navigation keys */
  kVirtualHomeKey = 0x073,
  kVirtualEndKey = 0x002,
  kVirtualPageUpKey = 0x074,
  kVirtualPageDownKey = 0x079,
  kVirtualLeftArrowKey = 0x07B,
  kVirtualRightArrowKey = 0x07C,
  kVirtualUpArrowKey = 0x07E,
  kVirtualDownArrowKey = 0x07D

};

enum {
  kPublicBetaVirtualLeftArrowKey = 0x03B,  /* Public Beta Only */
  kPublicBetaVirtualRightArrowKey = 0x03C, /* Public Beta Only */
  kPublicBetaVirtualUpArrowKey = 0x03E,    /* Public Beta Only */
  kPublicBetaVirtualDownArrowKey = 0x03D,  /* Public Beta Only */
  kPublicBetaVirtualCapsLockKey = 0x03D,   /* Public Beta Only */
  kPublicBetaVirtualHelpKey = 0x03D        /* Public Beta Only */
};

enum {
  kTableCountOffset = 256 + 2,
  kFirstTableOffset = 256 + 4,
  kTableSize = 128
};
