// [AUTO_HEADER]

#import <Cocoa/Cocoa.h>

/*!
        @class CVToolTipController
        @abstract The tooltip window
*/

@interface CVToolTipController : NSWindowController {
  IBOutlet id _messageTextField;
  float _lineHeight;
}

/*!
        @method showToolTip:atPoint:
        @abstract Shows the tooltip window with specified content
        and position.
        @param toolTip The text content of the tooltip window.
        @param point The position of the tooltip window.
*/
- (void)showToolTip:(NSString*)toolTip atPoint:(NSPoint)point;
/*!
        @method setToolTip:
        @abstract Sets the text content of the tooltip window.
        @param toolTip The text content of the tooltip window.
*/
- (void)setToolTip:(NSString*)toolTip;
/*!
        @method clear
        @abstract Clears the text content of the tooltip window.
*/
- (void)clear;
/*!
        @method show
        @abstract Shows the tooltip window.
*/
- (void)show;
/*!
        @method show
        @abstract Hides the tooltip window.
*/
- (void)hide;

@end
