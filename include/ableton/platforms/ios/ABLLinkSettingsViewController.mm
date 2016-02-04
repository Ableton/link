// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#import "ABLLinkSettingsViewController.h"
#import "ABLLinkAggregate.h"


@implementation ABLLinkSettingsViewController

+ (id)instance:(ABLLinkRef)ablLink {
  if (ablLink)
  {
    return ablLink->mpSettingsViewController;
  }
  else
  {
    return nil;
  }
}

- (id)init {
  return nil;
}

@end
