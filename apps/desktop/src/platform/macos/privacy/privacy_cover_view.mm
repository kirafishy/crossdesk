/* Copyright (c) 2026 by DI JUNKUN, All Rights Reserved. */

#include "privacy_cover_view.h"

#include <algorithm>
#include <cmath>

namespace {
NSImage* LoadPrivacyLogo() {
  // The macOS installer currently packages the icon as "crossedesk.icns".
  for (NSString* name in @[@"crossdesk", @"crossedesk"]) {
    NSString* path = [NSBundle.mainBundle pathForResource:name ofType:@"icns"];
    if (path) {
      NSImage* image = [[NSImage alloc] initWithContentsOfFile:path];
      if (image.valid) return image;
    }
  }
  // Support unbundled development builds, including launches from outside
  // the repository. No source-tree path is embedded in the executable.
  NSString* resource = @"apps/desktop/resources/macos/crossdesk.icns";
  NSString* directory = NSBundle.mainBundle.executablePath.stringByDeletingLastPathComponent;
  NSMutableArray<NSString*>* paths = [NSMutableArray arrayWithObject:
      [NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:resource]];
  for (int depth = 0; directory.length && depth <= 4; ++depth) {
    [paths addObject:[directory stringByAppendingPathComponent:resource]];
    directory = directory.stringByDeletingLastPathComponent;
  }
  for (NSString* path in paths) {
    NSImage* image = [[NSImage alloc] initWithContentsOfFile:path.stringByStandardizingPath];
    if (image.valid) return image;
  }
  return nil;
}

NSAttributedString* Text(NSString* value, CGFloat size, NSFontWeight weight, NSColor* color) {
  NSMutableParagraphStyle* paragraph = [[NSMutableParagraphStyle alloc] init];
  paragraph.lineBreakMode = NSLineBreakByWordWrapping;
  return [[NSAttributedString alloc] initWithString:value attributes:@{
      NSFontAttributeName: [NSFont systemFontOfSize:size weight:weight],
      NSForegroundColorAttributeName: color,
      NSParagraphStyleAttributeName: paragraph}];
}
constexpr NSStringDrawingOptions kTextOptions =
    NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingUsesFontLeading;

CGFloat TextHeight(NSAttributedString* text, CGFloat width) {
  return std::ceil([text boundingRectWithSize:NSMakeSize(width, CGFLOAT_MAX)
                                    options:kTextOptions].size.height);
}

NSAttributedString* BrandText(CGFloat scale) {
  NSFont* font = [NSFont fontWithName:@"Arial-BoldMT" size:29 * scale];
  if (!font) font = [NSFont boldSystemFontOfSize:29 * scale];
  auto* brand = [[NSMutableAttributedString alloc] initWithString:@"CrossDesk"
      attributes:@{NSFontAttributeName: font,
                   NSForegroundColorAttributeName:
                       [NSColor colorWithSRGBRed:69.0 / 255 green:135.0 / 255
                                           blue:219.0 / 255 alpha:1]}];
  [brand addAttribute:NSForegroundColorAttributeName
                value:[NSColor colorWithSRGBRed:37.0 / 255 green:83.0 / 255
                                          blue:151.0 / 255 alpha:1]
                range:NSMakeRange(5, 4)];
  [brand addAttribute:NSKernAttributeName value:@(scale) range:NSMakeRange(4, 1)];
  return brand;
}
}

@interface CrossDeskPrivacyCoverView : NSView
@property(nonatomic, strong) NSImage* logo;
@property(nonatomic, copy) NSString* unlockHint;
@property(nonatomic) BOOL allowLocalUnlock;
@end

@implementation CrossDeskPrivacyCoverView
- (BOOL)isOpaque { return YES; }
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirtyRect {
  [NSColor.blackColor setFill];
  NSRectFill(self.bounds);
  const CGFloat width = NSWidth(self.bounds), height = NSHeight(self.bounds);
  if (width <= 0 || height <= 0) return;
  // Match the Windows cover's sizes, colors and bottom-left alignment.
  // AppKit coordinates are points; Retina backing scale is applied by AppKit.
  const CGFloat scale = std::min({CGFloat(1), width / 768, height / 200});
  const CGFloat margin = 32 * scale, text_width = 398 * scale;
  NSColor* foreground = [NSColor colorWithSRGBRed:245.0 / 255 green:247.0 / 255
                                           blue:250.0 / 255 alpha:1];
  // Match the horizontal, two-tone wordmark in docs/images/crossdesk-banner.svg.
  NSAttributedString* brand = BrandText(scale);
  const CGFloat logo_size = 48 * scale;
  const CGFloat brand_text_width = std::ceil(brand.size.width);
  const CGFloat brand_text_height = std::ceil(brand.size.height);
  const CGFloat brand_width = logo_size + 8 * scale + brand_text_width;
  const CGFloat bottom = height - margin;
  const CGFloat logo_top = bottom - logo_size;
  [self.logo drawInRect:NSMakeRect(margin, logo_top, logo_size, logo_size)
              fromRect:NSZeroRect operation:NSCompositingOperationSourceOver
              fraction:1 respectFlipped:YES hints:@{NSImageHintInterpolation: @(NSImageInterpolationHigh)}];
  [brand drawWithRect:NSMakeRect(margin + logo_size + 8 * scale,
                                bottom - brand_text_height,
                                brand_text_width, brand_text_height)
             options:kTextOptions];
  if (!self.allowLocalUnlock) return;
  NSColor* muted = [NSColor colorWithSRGBRed:181.0 / 255 green:190.0 / 255
                                      blue:204.0 / 255 alpha:1];
  NSAttributedString* hint = Text(self.unlockHint, 16 * scale, NSFontWeightRegular, muted);
  NSAttributedString* shortcut = Text(@"Control + Option + Shift + Esc", 24 * scale,
                                      NSFontWeightSemibold, foreground);
  const CGFloat hint_height = TextHeight(hint, text_width);
  const CGFloat shortcut_height = TextHeight(shortcut, text_width);
  const CGFloat text_left = margin + brand_width + 24 * scale;
  [hint drawWithRect:NSMakeRect(text_left, bottom - shortcut_height - 12 * scale - hint_height,
                               text_width, hint_height) options:kTextOptions];
  [shortcut drawWithRect:NSMakeRect(text_left, bottom - shortcut_height,
                                   text_width, shortcut_height) options:kTextOptions];
}
@end

namespace crossdesk {
void UpdateMacPrivacyCoverView(NSView* view, NSString* hint, bool allow_local_unlock) {
  auto* cover = static_cast<CrossDeskPrivacyCoverView*>(view);
  cover.unlockHint = hint;
  cover.allowLocalUnlock = allow_local_unlock;
  cover.needsDisplay = YES;
}
NSView* CreateMacPrivacyCoverView(NSRect frame, NSString* unlock_hint, bool allow_local_unlock) {
  NSImage* logo = LoadPrivacyLogo();
  if (!logo) return nil;
  auto* view = [[CrossDeskPrivacyCoverView alloc] initWithFrame:frame];
  view.logo = logo;
  view.allowLocalUnlock = allow_local_unlock;
  view.unlockHint = unlock_hint ?: @"To turn off the privacy screen, press";
  view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  return view;
}
}
