// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef __APPLE__

#include "GlobalSettings.hpp"

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>

#if TARGET_OS_IPHONE
#import <GameController/GCMouse.h>
#else
#import <AppKit/AppKit.h>
#endif

GlobalSettings::ScrollBars
GlobalSettings::GetScrollBars() noexcept
{
  /* System Settings / General / Appearance / "Show scroll bars" is
     stored in the global domain; the key does not exist on iOS, and
     it is absent on macOS as well while the setting is "Automatic" */
  NSString *const preference = [[NSUserDefaults standardUserDefaults]
                                 stringForKey:@"AppleShowScrollBars"];

  if ([preference isEqualToString:@"Always"])
    return ScrollBars::ALWAYS;

  if ([preference isEqualToString:@"WhenScrolling"])
    return ScrollBars::WHEN_SCROLLING;

  /* "Automatic" means: overlay indicators for touch and trackpad
     scrolling, and a permanent scroll bar as soon as a mouse is
     attached */

#if TARGET_OS_IPHONE
  if (@available(iOS 14.0, *))
    return GCMouse.current != nil
      ? ScrollBars::ALWAYS
      : ScrollBars::WHEN_SCROLLING;

  return ScrollBars::WHEN_SCROLLING;
#else
  /* AppKit has already resolved "Automatic" for us */
  return NSScroller.preferredScrollerStyle == NSScrollerStyleLegacy
    ? ScrollBars::ALWAYS
    : ScrollBars::WHEN_SCROLLING;
#endif
}

#endif // __APPLE__
