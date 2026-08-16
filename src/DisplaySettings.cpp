// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DisplaySettings.hpp"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

void
DisplaySettings::SetDefaults()
{
  orientation = DisplayOrientation::DEFAULT;
  cursor_size = 1;
  invert_cursor_colors = false;
#if defined(__APPLE__) && TARGET_OS_IPHONE
  /* iOS has always laid out XCSoar inside the safe area; keep that,
     or an update would silently move every existing installation
     behind the status bar and the display cutout */
  full_screen = false;
#else
  full_screen = true;
#endif
#ifdef KOBO
  display_type = DisplayType::E_INK;
#else
  display_type = DisplayType::LCD;
#endif
}
