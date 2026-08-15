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

  /**
   * The same outside ring for the selected button, in a darker
   * shade.  Selected and focused share their face color, so the
   * ring is what tells them apart.
   */
  Color selected_ring_color;

  struct {
    Color color;
    Brush brush;

    /**
     * Face of a disabled button; a step towards the page
     * background, so it stops looking like a raised card.
     */
    Color background_color;

    /**
     * Border of a disabled button.  It carries the page background
     * color, which trims the face by the border width and leaves
     * no visible outline.
     */
    Color ring_color;
  } disabled;

  void Initialise(const Font &_font, bool dark_mode = false);
};
