// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#import <UIKit/UIKit.h>
#import "ABLLink.h"


/** Settings view controller that provides users with the ability to
    to view Link status and modify Link-related settings. Clients
    can integrate this view controller into their GUI as they see
    fit, but it is recommended that it be presented as a popover.
*/

@interface ABLLinkSettingsViewController : UIViewController

+ (id)instance:(ABLLinkRef)ablLink;

@end
