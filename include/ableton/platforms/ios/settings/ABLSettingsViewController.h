// Copyright: 2015, Ableton AG, Berlin. All rights reserved.
#pragma once

#import <UIKit/UIKit.h>

#define ABLLinkEnabledKey @"ABLLinkEnabledKey"
#define ABLNotificationEnabledKey @"ABLNotificationEnabled"

struct ABLLink;

@interface ABLSettingsViewController : UIViewController <UITableViewDataSource, UITableViewDelegate>

-(id)initWithLink:(ABLLink *)link;
-(void)deinit;
-(void)setNumberOfPeers:(size_t)peers;

@end
