// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GestureLook.hpp"
#include "Screen/Layout.hpp"
#include "Asset.hpp"

void
GestureLook::Initialise()
{
  if (IsDithered()) {
    /* colour is not useful on e-ink screens */
    color = COLOR_BLACK;
    invalid_color = COLOR_GRAY;
  } else {
    color = Color(0xd8, 0x3a, 0x2e);
    invalid_color = Color(0x8c, 0x8c, 0x8c);
  }

  width = Layout::ScalePenWidth(6);
  tip_radius = Layout::ScalePenWidth(10);

  pen.Create(width, color);
  invalid_pen.Create(width, invalid_color);
}
