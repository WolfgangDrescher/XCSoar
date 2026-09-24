// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GestureLook.hpp"
#include "Screen/Layout.hpp"
#include "Asset.hpp"

#include <algorithm>

void
GestureLook::Initialise()
{
  if (IsDithered()) {
    /* colour is not useful on e-ink screens */
    color = COLOR_BLACK;
    invalid_color = COLOR_GRAY;
  } else {
    /* red stands out on the map, which is mostly green, yellow,
       brown and blue; a bit darker than pure red, so it is not
       glaring */
    color = Color(0xe0, 0x1b, 0x1b);

    /* grey: "not (yet) recognised"; opaque, so it stands out like
       the red and the shadow does not show through */
    invalid_color = Color(0xa8, 0xa8, 0xa8);
  }

  outline_color = COLOR_WHITE;

  width = Layout::ScalePenWidth(5);
  /* no outline: the shadow alone sets the line off the map */
  outline_width = 0;
}
