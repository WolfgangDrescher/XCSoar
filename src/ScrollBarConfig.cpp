// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ScrollBarConfig.hpp"
#include "UISettings.hpp"
#include "GlobalSettings.hpp"
#include "ui/control/ScrollBar.hpp"
#include "Asset.hpp"

[[gnu::pure]]
static ScrollBar::Style
ResolveScrollBarStyle(UISettings::ScrollBars setting) noexcept
{
  switch (setting) {
  case UISettings::ScrollBars::WHEN_SCROLLING:
    return ScrollBar::Style::WHEN_SCROLLING;

  case UISettings::ScrollBars::ALWAYS:
    return ScrollBar::Style::ALWAYS;

  case UISettings::ScrollBars::AUTO:
    break;
  }

  /* e-paper is too slow to fade an indicator in and out, and its
     ghosting makes a permanent scroll bar the better choice */
  if (HasEPaper())
    return ScrollBar::Style::ALWAYS;

  switch (GlobalSettings::GetScrollBars()) {
  case GlobalSettings::ScrollBars::WHEN_SCROLLING:
    return ScrollBar::Style::WHEN_SCROLLING;

  case GlobalSettings::ScrollBars::ALWAYS:
    return ScrollBar::Style::ALWAYS;

  case GlobalSettings::ScrollBars::UNKNOWN:
    break;
  }

  /* the operating system has no preference: on a touch screen, the
     content itself is dragged and a scroll bar would only take away
     space from it */
  return HasTouchScreen()
    ? ScrollBar::Style::WHEN_SCROLLING
    : ScrollBar::Style::ALWAYS;
}

void
ApplyScrollBars(const UISettings &settings) noexcept
{
  ScrollBar::SetGlobalStyle(ResolveScrollBarStyle(settings.scroll_bars));
}
