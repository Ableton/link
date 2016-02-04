// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#import "ABLFontManager.h"
#import <CoreText/CoreText.h>
#import "FontData.h"
#include <tuple>

@implementation ABLFontManager

+(UIFont*)getABLFont:(ABLFontType)font withSize:(CGFloat)fontSize
{
  std::pair<unsigned char*,unsigned long> fontBinary;
  switch (font)
  {
    case ABLFONT_LIGHT:
      fontBinary = std::make_pair(AbletonSans_Light_otf, AbletonSans_Light_otf_len);
      break;
    case ABLFONT_EXTRALIGHT:
      fontBinary = std::make_pair(AbletonSans_ExtraLight_otf, AbletonSans_ExtraLight_otf_len);
      break;
    case ABLFONT_BOOK:
      fontBinary = std::make_pair(AbletonSans_Book_otf, AbletonSans_Book_otf_len);
      break;
  }

  UIFont *ablFont;
  NSData *inData = [NSData dataWithBytes:fontBinary.first length:fontBinary.second];
  CFErrorRef error;
  CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)inData);
  CGFontRef rFont = CGFontCreateWithDataProvider(provider);
  NSString *fontName = (__bridge NSString *)CGFontCopyPostScriptName(rFont);
  if (! CTFontManagerRegisterGraphicsFont(rFont, &error)) {
    CFStringRef errorDescription = CFErrorCopyDescription(error);
    CFRelease(errorDescription);
  }
  CFRelease(rFont);
  CFRelease(provider);

  ablFont = [UIFont fontWithName:fontName size:fontSize];

  return ablFont;
}

@end
