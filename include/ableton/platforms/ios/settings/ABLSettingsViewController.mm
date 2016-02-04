// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#import "ABLSettingsViewController.h"
#include <platforms/ios/ABLLinkAggregate.h>
#include <functional>
#include <tuple>
#include <string>
#include <array>

namespace
{

void initUserDefaultFlag(NSString* key, BOOL defaultVal)
{
  if (![[NSUserDefaults standardUserDefaults] objectForKey:key])
  {
    [[NSUserDefaults standardUserDefaults] setBool:defaultVal forKey:key];
  }
}

} // unnamed


@interface ABLSettingsViewController ()
{
  ABLLink *_ablLink;
  UITableView *ablSettingsView;
  size_t connectedPeers;
}
@property (nonatomic, retain) NSArray *topSettingsCells;
@property (nonatomic, retain) NSMutableArray *middleDynamicCells;
@property (nonatomic, retain) NSArray *bottomDescriptionCells;
-(UIView*)bottomLineSeparatorForCell:(UITableViewCell *) cell;
-(UIView*)topLineSeparatorForCell:(UITableViewCell *) cell;
-(void)openLinkPage:(id)sender;
@end

// UI definitions and constants
const int kTableTopPadding = 40;
const int kLeftSidePadding = 15;
const int kStandardCellHeight = 44;
const int kCellHeightForDescriptionText = 10;
const int kConnectedAppsSectionHeaderHeight = 100;
static UIColor *kBackgroundColor = [UIColor colorWithRed:235/255.0f green:235/255.0f blue:235/255.0f alpha:1.0f];
static NSString *cellIdentifier = @"Cell";
static NSString *kDescriptionText = @"Link allows you to play in time with other Link-enabled apps that are on the same network.\n \nTo create or join a session, enable Link.";
static NSString *kBrowsingText = @"Browsing for Link-enabled apps...";
static NSString *kLearnMoreText = @"Learn more at Ableton.com/Link";

const float kSeparatorWidth = 0.5f;

@implementation ABLSettingsViewController
@synthesize topSettingsCells = _topSettingsCells, middleDynamicCells = _middleDynamicCells, bottomDescriptionCells = _bottomDescriptionCells;

-(void)viewDidLoad
{
  [super viewDidLoad];
  self.view.backgroundColor = kBackgroundColor;
  self.view.layer.zPosition = 999;
  self.title = @"Ableton Link";

  ablSettingsView = [[UITableView alloc] initWithFrame:CGRectMake(0, 0,
                                                                  self.view.frame.size.width,
                                                                  self.view.frame.size.height)
                                                 style:UITableViewStyleGrouped];
  ablSettingsView.dataSource = self;
  ablSettingsView.delegate = self;
  ablSettingsView.scrollEnabled = false;
  ablSettingsView.backgroundColor = [UIColor clearColor];
  [ablSettingsView setSeparatorStyle:UITableViewCellSeparatorStyleNone];

  // iOS 9: Disable introduced automatic margins in tableview cells
  if ([ablSettingsView respondsToSelector:@selector(cellLayoutMarginsFollowReadableWidth)])
  {
    ablSettingsView.cellLayoutMarginsFollowReadableWidth = NO;
  }
  self.edgesForExtendedLayout = UIRectEdgeNone; // fixes automatic insets in tableview

  [self.view addSubview:ablSettingsView];
  [ablSettingsView setScrollEnabled:YES];
  [ablSettingsView setContentInset:UIEdgeInsetsMake(kTableTopPadding, 0, 0, 0)];
}

-(void)openLinkPage:(id)sender
{
#pragma unused(sender)
   [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"http://www.ableton.com/link"]];
}

-(void)didReceiveMemoryWarning
{
  [super didReceiveMemoryWarning];
  // Dispose of any resources that can be recreated.
}

-(void)setNumberOfPeers:(size_t)peers
{
  connectedPeers = peers;
  [ablSettingsView reloadData];
}

-(NSArray*)topSettingsCells
{
  UITableViewCell *enableDisableCell = [[UITableViewCell alloc]
                                        initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:cellIdentifier];

  enableDisableCell.backgroundView = [[UIView alloc] init];
  [enableDisableCell.backgroundView setBackgroundColor:[UIColor whiteColor]];
  enableDisableCell.textLabel.text = @"Ableton Link";
  UISwitch *switchview = [[UISwitch alloc] initWithFrame:CGRectZero];
  BOOL enabled = [self isEnabled];
  switchview.on = enabled;
  [switchview addTarget:self action:@selector(enableLink:) forControlEvents:UIControlEventValueChanged];
  enableDisableCell.accessoryView = switchview;
  enableDisableCell.separatorInset = UIEdgeInsetsMake(0, kLeftSidePadding, 20, 0);
  enableDisableCell.tag = 1;
  [enableDisableCell.contentView addSubview:[self topLineSeparatorForCell:enableDisableCell]];
  [enableDisableCell addSubview:[self bottomLineSeparatorForCell:enableDisableCell]];
  if (enabled)
  {
    UITableViewCell *persistantStatusCell = [[UITableViewCell alloc]
                                             initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:cellIdentifier];

    persistantStatusCell.backgroundView = [[UIView alloc] init];
    [persistantStatusCell.backgroundView setBackgroundColor:[UIColor whiteColor]];
    persistantStatusCell.textLabel.text = @"In-app notifications";
    persistantStatusCell.tag = 1;
    persistantStatusCell.detailTextLabel.text = @"Get notified when apps join or leave";
    persistantStatusCell.detailTextLabel.textColor = [UIColor grayColor];
    persistantStatusCell.detailTextLabel.font = [UIFont systemFontOfSize:12];
    UISwitch *notificationSwitchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    notificationSwitchView.on = [[NSUserDefaults standardUserDefaults] boolForKey:ABLNotificationEnabledKey];
    [notificationSwitchView addTarget:self action:@selector(enableNotifications:) forControlEvents:UIControlEventValueChanged];
    persistantStatusCell.accessoryView = notificationSwitchView;
    persistantStatusCell.separatorInset = UIEdgeInsetsMake(0, kLeftSidePadding, 20, 0);

    [persistantStatusCell addSubview:[self bottomLineSeparatorForCell:persistantStatusCell]];
    _topSettingsCells = [[NSArray alloc] initWithObjects:enableDisableCell, persistantStatusCell, nil];
  }
  else
  {
    _topSettingsCells = [[NSArray alloc] initWithObjects:enableDisableCell, nil];
  }

  return _topSettingsCells;
}

-(NSArray*)bottomDescriptionCells
{

  NSMutableArray *mutableTableCells = [[NSMutableArray alloc] init];

  if (![self isEnabled])
  {
    UITableViewCell *panamaDescriptionCell =
    [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cellIdentifier];

    panamaDescriptionCell.textLabel.text = kDescriptionText;
    panamaDescriptionCell.textLabel.font =  [UIFont systemFontOfSize:13.0f weight:UIFontWeightLight];
    panamaDescriptionCell.textLabel.textColor = [UIColor grayColor];
    panamaDescriptionCell.textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    panamaDescriptionCell.textLabel.numberOfLines = 0;
    panamaDescriptionCell.textLabel.textAlignment = NSTextAlignmentLeft;
    panamaDescriptionCell.tag = 13;
    panamaDescriptionCell.backgroundColor = [UIColor clearColor];
    panamaDescriptionCell.separatorInset = UIEdgeInsetsMake(0, kLeftSidePadding, 20, 0);
    [mutableTableCells addObject:panamaDescriptionCell];
  }

  _bottomDescriptionCells = [mutableTableCells copy];

  return _bottomDescriptionCells;
}

-(NSMutableArray *)middleDynamicCells
{
  if (!_middleDynamicCells)
  {
    _middleDynamicCells = [[NSMutableArray alloc] init];
  }

  // Disposing all cells
  [_middleDynamicCells removeAllObjects];

  if ([self isEnabled])
  {
    // dynamically add content here
    // table cells will slide in
    UITableViewCell *persistantStatusCell = [[UITableViewCell alloc]
                                             initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:cellIdentifier];

    [persistantStatusCell.contentView addSubview:[self bottomLineSeparatorForCell:persistantStatusCell]];
    [persistantStatusCell.contentView addSubview:[self topLineSeparatorForCell:persistantStatusCell]];
    persistantStatusCell.backgroundView = [[UIView alloc] init];
    [persistantStatusCell.backgroundView setBackgroundColor:[UIColor whiteColor]];
    NSString *linkText = connectedPeers == 1 ? @"Connected to 1 app" : [NSString stringWithFormat:@"Connected to %zu apps",
                                                                     connectedPeers];
    persistantStatusCell.textLabel.text = linkText;
    persistantStatusCell.separatorInset = UIEdgeInsetsMake(0, kLeftSidePadding, 20, 0);
    [_middleDynamicCells addObject:persistantStatusCell];
  }

  return _middleDynamicCells;
}

-(void)enableLink:(UISwitch*)sender
{
    [[NSUserDefaults standardUserDefaults] setBool:sender.on forKey:ABLLinkEnabledKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

-(void)enableNotifications:(UISwitch*)sender
{
    [[NSUserDefaults standardUserDefaults] setBool:sender.on forKey:ABLNotificationEnabledKey];
}

-(id)initWithLink:(ABLLink *)ablLink
{
  if ((self = [super init]))
  {
    // Link is disabled by default but notifications are enabled by default
    initUserDefaultFlag(ABLLinkEnabledKey, NO);
    initUserDefaultFlag(ABLNotificationEnabledKey, YES);
    _ablLink = ablLink;
    [self updateEnabled];

    [[NSUserDefaults standardUserDefaults] addObserver:self forKeyPath:ABLLinkEnabledKey options:(NSKeyValueObservingOptionNew|NSKeyValueObservingOptionOld) context:nil];
  }

  return self;
}

-(void)deinit
{
    [[NSUserDefaults standardUserDefaults] removeObserver:self forKeyPath:ABLLinkEnabledKey context:NULL];
    [[NSUserDefaults standardUserDefaults] synchronize];
    _ablLink = NULL;
}

-(BOOL)isEnabled
{
  return _ablLink->mEnabled;
}

- (void)updateEnabled
{
  _ablLink->mEnabled =
    [[NSUserDefaults standardUserDefaults] boolForKey:ABLLinkEnabledKey];
  _ablLink->updateEnabled();
}

-(void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
#pragma unused(change)
#pragma unused(context)
  if ((object == [NSUserDefaults standardUserDefaults]) && [keyPath isEqualToString:ABLLinkEnabledKey]) {
    [self updateEnabled];
    _ablLink->mpCallbacks->mIsEnabledCallback([self isEnabled]);
    // reset instance variable to zero since we are not getting a callback for this
    connectedPeers = 0;
    // reload dynamic elements
    [ablSettingsView reloadData];
  }
}

-(void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
}

-(void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];

  // developer has put us in a different parentview than before
  // need to reload to re-set the table cell separators
  if (ablSettingsView.frame.size.width != self.view.frame.size.width)
  {
    [ablSettingsView reloadData];
  }
}

-(void)viewDidLayoutSubviews
{
  // we need to reset the settings frame size because it
  // will change right before appearing
  ablSettingsView.frame = CGRectMake(0, 0,
                                     self.view.frame.size.width,
                                     self.view.frame.size.height);
}

-(void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
}

#pragma mark - UI Table View

-(CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
#pragma unused(tableView)
  if (indexPath.section == 2)
  {
    CGSize maximumSize = CGSizeMake(self.view.frame.size.width, CGFLOAT_MAX);
    UIFont *descriptionTextFont =  [UIFont systemFontOfSize:13.0f weight:UIFontWeightLight];
    CGSize descriptionTextSize = [kDescriptionText sizeWithFont:descriptionTextFont
                             constrainedToSize:maximumSize
                             lineBreakMode:NSLineBreakByWordWrapping];
    return descriptionTextSize.height + kCellHeightForDescriptionText;
  }
  else
  {
    return kStandardCellHeight;
  }
}

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
#pragma unused(tableView)
  switch (section)
  {
    case 0:
      return static_cast<NSInteger>(self.topSettingsCells.count);
    case 1:
      return static_cast<NSInteger>(self.middleDynamicCells.count);
    case 2:
      return static_cast<NSInteger>(self.bottomDescriptionCells.count);
    default:
      return 0;
  }
}

-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
#pragma unused(tableView)
  UITableViewCell *cell;
  switch (indexPath.section)
  {
    case 0:
      cell = [self.topSettingsCells objectAtIndex:static_cast<NSUInteger>(indexPath.row)];
      break;
    case 1:
      cell = [self.middleDynamicCells objectAtIndex:static_cast<NSUInteger>(indexPath.row)];
      break;
    case 2:
      cell = [self.bottomDescriptionCells objectAtIndex:static_cast<NSUInteger>(indexPath.row)];
      break;
  }
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  return cell;
}

-(NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
#pragma unused(tableView)
  return 3;
}

-(NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
#pragma unused(tableView)
  NSString *sectionName;
  switch (section)
  {
    case 0:
      break;
    case 1:
      if ([self isEnabled])
      {
        sectionName = @("CONNECTED APPS");
      }
      break;
    default:
      sectionName = @"";
      break;
  }
  return sectionName;
}

-(UIView *)tableView:(UITableView *)tableView viewForHeaderInSection:(NSInteger)section
{
  if ([self isEnabled] && section == 1)
  {
    UILabel *headerLabel = [[UILabel alloc] init];
    CGSize textSize = [[self tableView:tableView titleForHeaderInSection:section] sizeWithAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:12]}];
    headerLabel.frame = CGRectMake(ablSettingsView.layoutMargins.left, 75, textSize.width, 20);
    headerLabel.font = [UIFont systemFontOfSize:12];
    headerLabel.textColor = [UIColor lightGrayColor];
    headerLabel.text = [self tableView:tableView titleForHeaderInSection:section];

    UIView *headerView = [[UIView alloc] init];
    [headerView addSubview:headerLabel];


    [headerView sizeToFit];

    return headerView;
  }
  else
  {
    return [[UIView alloc] initWithFrame:CGRectZero];
  }
}

- (UIView *)tableView:(UITableView *)tableView viewForFooterInSection:(NSInteger)section
{
#pragma unused(tableView)
  if ([self isEnabled] && section == 1)
  {
    UILabel *headerLabel = [[UILabel alloc] init];
    CGSize textSize = [kBrowsingText sizeWithAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:13]}];
    headerLabel.frame = CGRectMake(ablSettingsView.layoutMargins.left, 10, textSize.width, 14);
    headerLabel.font = [UIFont systemFontOfSize:13];
    headerLabel.textColor = [UIColor lightGrayColor];
    headerLabel.text = kBrowsingText;

    UIView *headerView = [[UIView alloc] init];
    [headerView addSubview:headerLabel];

    UIActivityIndicatorView *activityIndicator;
    activityIndicator = [[UIActivityIndicatorView alloc]initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
    activityIndicator.frame = CGRectMake(headerLabel.frame.origin.x +
                                         headerLabel.frame.size.width +
                                         5,
                                         headerLabel.frame.origin.y,
                                         15, 15);

    [activityIndicator startAnimating];
    // Resizing through CoreGraphics because the size is fixed
    CGAffineTransform resizeFactor = CGAffineTransformMakeScale(0.8f, 0.8f);
    activityIndicator.transform = resizeFactor;
    [headerView addSubview: activityIndicator];

    [headerView sizeToFit];

    return headerView;
  }
  if ((section == 2 && ![self isEnabled]) || (section == 0 && [self isEnabled]))
  {
    CGSize textSize = [kLearnMoreText sizeWithAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:13]}];
    UIButton *tblfooterLabel = [[UIButton alloc] initWithFrame:CGRectMake(ablSettingsView.layoutMargins.left, 10,
                                                             textSize.width,
                                                             14)];
    tblfooterLabel.titleLabel.textColor = [UIColor grayColor];
    tblfooterLabel.backgroundColor = [ablSettingsView backgroundColor];
    tblfooterLabel.font = [UIFont systemFontOfSize:13.0f weight:UIFontWeightLight];
    NSMutableAttributedString *str = [[NSMutableAttributedString alloc] initWithString:kLearnMoreText];
    [str addAttribute:NSForegroundColorAttributeName value:[UIColor blueColor] range:NSMakeRange(14,16)];
    [tblfooterLabel setAttributedTitle:str forState:UIControlStateNormal];
    tblfooterLabel.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
    [tblfooterLabel addTarget:self action:@selector(openLinkPage:) forControlEvents:UIControlEventTouchUpInside];
    UIView *footerLabelView = [[UIView alloc] init];
    [footerLabelView addSubview:tblfooterLabel];

    [footerLabelView sizeToFit];
    return footerLabelView;

  }
  else
  {
    return [[UIView alloc] initWithFrame:CGRectZero];;
  }
}


// Handle rotation
-(void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  ablSettingsView.contentSize = size;

  [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context)
   {
#pragma unused(context)
     self->ablSettingsView.frame = CGRectMake(0, 0,
                                              self.view.frame.size.width,
                                              self.view.frame.size.height);
   }
   completion:^(id<UIViewControllerTransitionCoordinatorContext> context)
   {
#pragma unused(context)
     [self->ablSettingsView reloadData];
   }];
}

// we need these delegates to accomplish design consistenct across iOS 7 and 8
-(CGFloat)tableView:(UITableView *)tableView heightForFooterInSection:(NSInteger)section
{
#pragma unused(tableView)
  if (section == 1 && [self isEnabled])
  {
    return 28;
  }
  else
  {
    return CGFLOAT_MIN;
  }
}

-(CGFloat)tableView:(UITableView *)tableView heightForHeaderInSection:(NSInteger)section
{
#pragma unused(tableView)
#pragma unused(section)
  if (section == 1 && [self isEnabled])
  {
    return kConnectedAppsSectionHeaderHeight;
  }
  else
  {
    return 4;
  }
}

// utility function for custom cell separators
-(UIView*)bottomLineSeparatorForCell:(UITableViewCell *) cell
{
  UIView *cellSeparator = [[UIView alloc] initWithFrame:CGRectMake(0, cell.contentView.frame.size.height - 0.5f,
                                                                   self.view.frame.size.width, kSeparatorWidth)];
  cellSeparator.backgroundColor = [UIColor lightGrayColor];

  return cellSeparator;
}

-(UIView*)topLineSeparatorForCell:(UITableViewCell *) cell
{
  UIView *cellSeparator = [[UIView alloc] initWithFrame:CGRectMake(cell.frame.origin.x, 0,
                                                                   self.view.frame.size.width, kSeparatorWidth)];

  cellSeparator.backgroundColor = [UIColor lightGrayColor];

  return cellSeparator;
}

@end
