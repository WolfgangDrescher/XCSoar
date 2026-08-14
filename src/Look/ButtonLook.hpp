// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Color.hpp"
#include "ui/canvas/Brush.hpp"

class Font;

struct ButtonLook {
  const Font *font;

  struct StateLook {
    Color foreground_color;
    Brush foreground_brush;

    Color background_color;

    /**
     * Background while the button is pressed down (the CSS `active`
     * state).
     */
    Color pressed_background_color;

    /**
     * Hairline border drawn on the face outline, like a Tailwind
     * inset ring.
     */
    Color ring_color;
  } standard, selected, focused;

  /**
   * Solid ring hugging the focused button face from the outside,
   * like a Tailwind `ring-3` in the palette's light primary:
   * lighter than the face, but saturated enough to read as a
   * defined contour, not a washed-out glow.
   */
  Color focus_ring_color;

  struct {
    Color color;
    Brush brush;
  } disabled;

  void Initialise(const Font &_font, bool dark_mode = false);
};
