// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "ABLNotificationView.h"
#import "ABLFontManager.h"
#import "../settings/ABLSettingsViewController.h"
#include <string>

@interface ABLLinkNotificationView : UIToolbar
// Notifications
@property (strong, nonatomic) UILabel *messageLabel;
@property (strong, nonatomic) NSTimer *timeToCloseMessage;
@property (strong, nonatomic) UIView *containerView;
@property (strong, nonatomic) UIBarButtonItem *notificationBarView;
-(void)showNotification:(ABLLinkNotificationType)notificationType withPeers:(size_t)peers;
-(void)hideNotification;
@end

namespace
{

struct NotificationMessage {
  std::string message;
  float red;
  float green;
  float blue;
  float alpha;
};

// Notification Appearance
const NSTimeInterval kShowDuration = 2.6;
const NSTimeInterval kAnimateShowDuration = 0.30;
const NSTimeInterval kAnimateHideDuration = 0.35;
UIColor * backgroundColor = [UIColor colorWithRed:64/255.0f green:211/255.0f blue:178/255.0f alpha:1.0f];
const float kHeaderFontSize = 16.0f;
int kPopOverHeight = 40;
int kMessageLabelHeightOffset = 0;
const int kPopOverTextInsetLeft = 0;

BOOL lastMessageFinished = YES;
NotificationMessage messageForNotificationType(ABLLinkNotificationType notification, size_t peers = 0)
{
  NotificationMessage msg;
  msg.message = "";

  switch (notification)
  {
  case ABLLINK_NOTIFY_CONNECTED:
      msg.message = (peers == 1) ? msg.message.append(std::to_string(peers) + " Link") :
                                   msg.message.append(std::to_string(peers) + " Links");
      msg.red = 255.0f/255.0f; msg.green = 250.0f/255.0f; msg.blue = 255.0f/255.0f; msg.alpha = 1.0f;
      return msg;
  case ABLLINK_NOTIFY_DISCOVERY:
      msg.message.append("Disconnected");
      msg.red = 253.0f/255.0f; msg.green = 146.0f/255.0f; msg.blue = 47.0f/255.0f; msg.alpha = 1.0f;
      return msg;
  case ABLLINK_NOTIFY_DISABLED:
      msg.message.append("Disabled");
      msg.red = 44.0f/255.0f; msg.green = 253.0f/255.0f; msg.blue = 254.0f/255.0f; msg.alpha = 1.0f;
      return msg;
  case ABLLINK_NOTIFY_ENABLED:
      msg.message.append("Enabled");
      msg.red = 44.0f/255.0f; msg.green = 252.0f/255.0f; msg.blue = 253.0f/255.0f; msg.alpha = 1.0f;
      return msg;
  }
}

// notification view singleton
ABLLinkNotificationView *notificationView()
{
  static ABLLinkNotificationView *view = [ABLLinkNotificationView new];
  return view;
}

} // unnamed namespace

// Externally exposed notification function
void ABLLinkNotificationsShowMessage(ABLLinkNotificationType notificationType, size_t peers)
{
  if([[NSUserDefaults standardUserDefaults] boolForKey:ABLNotificationEnabledKey] &&
      [UIApplication sharedApplication].applicationState == UIApplicationStateActive)
  {
    [notificationView() showNotification:notificationType withPeers:peers];
  }
}

@implementation ABLLinkNotificationView
{
  UIWindowLevel _statusBarLevel;
  BOOL _statusBarBefore;
  size_t _numberOfPeers;
}

@synthesize messageLabel, timeToCloseMessage, notificationBarView, containerView;

-(id)init
{
  if ((self = [super init]))
  {

    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];

    [[NSNotificationCenter defaultCenter]
     addObserver:self selector:@selector(orientationChanged:)
     name:UIDeviceOrientationDidChangeNotification
     object:[UIDevice currentDevice]];

    self.layer.zPosition = UIWindowLevelStatusBar;
    self.multipleTouchEnabled = NO;
    self.exclusiveTouch = NO;
    self.userInteractionEnabled = NO;

    // translucency
    self.opaque = NO;
    self.barStyle = UIBarStyleBlackTranslucent;

    [self orientationChanged:nil];
    self.containerView.hidden = YES;
    [[UIApplication sharedApplication].delegate.window addSubview:self];

    // storing status bar position from main application in order to
    // restore it after displaying notifications
    _statusBarLevel = [UIApplication sharedApplication].delegate.window.windowLevel;
  }
  return self;
}

-(void)showNotification:(ABLLinkNotificationType)notificationType withPeers:(size_t)peers
{
  // invalidate and remove potential previous message
  [self.timeToCloseMessage invalidate];
  [self.layer removeAllAnimations];

  if (self.frame.size.width != [UIApplication sharedApplication].keyWindow.frame.size.width)
  {
    [self resetLayoutWithWidth:[UIApplication sharedApplication].keyWindow.frame.size.width];
  }

  [UIApplication sharedApplication].delegate.window.windowLevel = UIWindowLevelStatusBar;

  self.messageLabel.text = [NSString stringWithCString:messageForNotificationType(notificationType, peers).message.c_str()
                                                encoding:NSUTF8StringEncoding];

  self.messageLabel.textAlignment = NSTextAlignmentCenter;

  self.messageLabel.textColor = [UIColor colorWithRed:messageForNotificationType(notificationType).red
                                                green:messageForNotificationType(notificationType).green
                                                blue:messageForNotificationType(notificationType).blue
                                                alpha:1.0f];

  self.containerView.hidden = NO;

  CGRect frame = self.frame;

  // check if last message has reached animation end, if not, update its current content and dismiss
  if (lastMessageFinished)
  {
    self.frame = CGRectMake(frame.origin.x, -frame.size.height, frame.size.width, frame.size.height);
    lastMessageFinished = NO;
    [UIView animateWithDuration:kAnimateShowDuration
                          delay:0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:^{
        self.frame = CGRectMake(frame.origin.x, 0, frame.size.width, frame.size.height);
      }
                     completion:^(BOOL finished) {
          #pragma unused (finished)
          self.timeToCloseMessage = [NSTimer scheduledTimerWithTimeInterval:kShowDuration
                                                                     target:self
                                                                   selector:@selector(hideNotification)
                                                                   userInfo:nil
                                                                    repeats:NO];
    }];
  }
  else
  {
    self.frame = CGRectMake(frame.origin.x, 0, frame.size.width, frame.size.height);
    self.timeToCloseMessage = [NSTimer scheduledTimerWithTimeInterval:kShowDuration
                                                               target:self
                                                             selector:@selector(hideNotification)
                                                             userInfo:nil
                                                              repeats:NO];
  }
}

-(void)hideNotification
{
  [UIView animateWithDuration:kAnimateHideDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
      CGRect frame = self.frame;
      self.frame = CGRectMake(frame.origin.x, -frame.size.height, frame.size.width, frame.size.height);
    }
                   completion:^(BOOL finished) {
       if (finished)
       {
        [UIApplication sharedApplication].delegate.window.windowLevel = self->_statusBarLevel;
        self.containerView.hidden = YES;
        lastMessageFinished = YES;
       }
       else
       {
         [self.timeToCloseMessage invalidate];
         self.timeToCloseMessage = [NSTimer scheduledTimerWithTimeInterval:kShowDuration
                                                                    target:self
                                                                  selector:@selector(hideNotification)
                                                                  userInfo:nil
                                                                   repeats:NO];
       }

  }];
}

-(void)orientationChanged:(NSNotification *)note
{
#pragma unused(note)
  UIDeviceOrientation orientation = [UIDevice currentDevice].orientation;
  CGSize frameSize = [UIApplication sharedApplication].keyWindow.frame.size;
  CGFloat width = frameSize.width;

  if (UIDeviceOrientationIsLandscape(orientation))
  {
    width = MAX(frameSize.width, frameSize.height);
  }
  else if (UIDeviceOrientationIsPortrait(orientation))
  {
    width = MIN(frameSize.width, frameSize.height);
  }
  [self resetLayoutWithWidth:width];
}

-(void)resetLayoutWithWidth:(CGFloat)width
{
  CGFloat height = kPopOverHeight;
  self.frame = CGRectMake(0, -height, width, height);

  if (!lastMessageFinished)
  {
    lastMessageFinished = YES;
  }
  // device orientation has changed, removing ourselves from the superviews
  [self.containerView removeFromSuperview];
  [self.messageLabel removeFromSuperview];

  // reinitialize UI elements with new frame coordinates
  self.containerView = [[UIView alloc] initWithFrame:self.frame];
  [self.containerView setBackgroundColor:backgroundColor];

  self.messageLabel =
  [[UILabel alloc] initWithFrame:CGRectMake(kPopOverTextInsetLeft, kMessageLabelHeightOffset, width, height - kMessageLabelHeightOffset)];
  [self.messageLabel setFont:[ABLFontManager getABLFont:ABLFONT_BOOK withSize:kHeaderFontSize]];

  self.containerView.hidden = YES;
  self.notificationBarView = [[UIBarButtonItem alloc] initWithCustomView:containerView];
  [self.containerView addSubview:self.messageLabel];
  [self setItems:[NSArray arrayWithObjects: notificationBarView, nil]];
}

-(void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Utility Functions

-(CGFloat)statusLabelWidthForLabel:(UILabel*)label
{
  CGFloat statusLabelWidth = [label.text boundingRectWithSize:label.frame.size
                                                      options:NSStringDrawingUsesLineFragmentOrigin
                                                   attributes:@{ NSFontAttributeName:label.font }
                                                      context:nil].size.width;
  return statusLabelWidth;
}


@end
