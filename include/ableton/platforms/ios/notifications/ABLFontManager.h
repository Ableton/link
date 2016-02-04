// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

typedef enum
{
  ABLFONT_EXTRALIGHT,
  ABLFONT_LIGHT,
  ABLFONT_BOOK
} ABLFontType;

@interface ABLFontManager : NSObject

+(UIFont*)getABLFont:(ABLFontType)font withSize:(CGFloat)fontSize;

@end
