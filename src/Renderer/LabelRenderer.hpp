// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "LabelStyle.hpp"

#include <cstdint>

struct PixelPoint;
struct PixelSize;
struct PixelRect;
class Canvas;
class LabelBlock;

void
RenderShadowedText(Canvas &canvas, const char *text,
                   PixelPoint p,
                   bool inverted) noexcept;

/**
 * Where LabelRenderer::Draw() puts a label: which point of the label
 * sits on the anchor.
 */
struct LabelPlacement {
  enum class HorizontalAlign : uint8_t {
    /** the anchor is on the left edge; the text runs to the right */
    LEFT,
    CENTER,
    /** the anchor is on the right edge */
    RIGHT,
  };

  enum class VerticalAlign : uint8_t {
    /** the anchor is on the top edge; the label hangs below it */
    TOP,
    MIDDLE,
    /** the anchor is on the bottom edge; the label stands above it */
    BOTTOM,
  };

  HorizontalAlign horizontal_align = HorizontalAlign::LEFT;
  VerticalAlign vertical_align = VerticalAlign::TOP;

  /**
   * Move the label into the clip rectangle if it would stick out.
   */
  bool move_in_view = false;
};

namespace LabelRenderer {

/**
 * Draw a label with the currently selected font.
 *
 * @param anchor the point the label is placed at, see
 * #LabelPlacement
 * @param clip_rc the rectangle the label must stay in (with
 * LabelPlacement::move_in_view)
 * @param label_block if not nullptr, the label is only drawn if it
 * does not overlap one drawn before
 * @return true if the label was drawn
 */
bool
Draw(Canvas &canvas, const char *text, PixelPoint anchor,
     const LabelStyle &style, LabelPlacement placement,
     const PixelRect &clip_rc,
     LabelBlock *label_block=nullptr) noexcept;

bool
Draw(Canvas &canvas, const char *text, PixelPoint anchor,
     const LabelStyle &style, LabelPlacement placement,
     PixelSize screen_size,
     LabelBlock *label_block=nullptr) noexcept;

} // namespace LabelRenderer
