// [AUTO_HEADER]

#import "CVNotifyWindow.h"

#import "CVNotifyController.h"

@implementation CVNotifyWindow

- (void)mouseDown:(NSEvent *)theEvent {
  [(CVNotifyController *)_controller fade];
}

@end
