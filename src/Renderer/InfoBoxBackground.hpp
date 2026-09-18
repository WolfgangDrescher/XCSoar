// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxes/InfoBoxSettings.hpp"
#include "ui/dim/Rect.hpp"

class Canvas;
class Color;
class Pen;

/**
 * The shape of the background of an InfoBox (or of the vario gauge
 * among the InfoBoxes) within its window.  The classic styles fill
 * the whole window; #InfoBoxSettings::BorderStyle::FLOATING and
 * #InfoBoxSettings::BorderStyle::DOCK set the box back from some
 * window edges and round the corners in between, so the map shows
 * through the gap.
 */
struct InfoBoxBackgroundShape {
  /**
   * How far the box is set back from the window edge on #edges.
   */
  unsigned margin = 0;

  /**
   * How far the box is set back on those of #edges which are also
   * outer edges of the block of InfoBoxes (#outer_edges).  Two
   * neighbouring boxes each contribute #margin to the gap between
   * them; the gap to the screen edge and to the map has only one
   * box on its side, so it needs the whole gap from that box.
   */
  unsigned outer_margin = 0;

  /**
   * The radius of the corners where two of #edges meet.
   */
  unsigned radius = 0;

  /**
   * The window edges the box is set back from, as BORDER* flags
   * (see InfoBoxes/Border.hpp).
   */
  unsigned edges = 0;

  /**
   * The window edges which are outer edges of the block of adjacent
   * InfoBoxes, as BORDER* flags; a subset of #edges.
   */
  unsigned outer_edges = 0;

  /**
   * Draw an outline along #edges?
   *
   * @see DrawInfoBoxOutline()
   */
  bool outline = false;

  /**
   * @param outer_edges the window edges which are outer edges of the
   * block of adjacent InfoBoxes, see InfoBoxLayout::GetOuterEdges()
   */
  [[gnu::pure]]
  static InfoBoxBackgroundShape For(InfoBoxSettings::BorderStyle style,
                                    unsigned outer_edges) noexcept;

  /**
   * The area within the window the box occupies.
   */
  [[gnu::pure]]
  PixelRect Inset(PixelRect rc) const noexcept;
};

/**
 * The base colour of a translucent box: a light or a dark grey,
 * depending on the theme, with the alpha applied by
 * #MakeTranslucent().
 */
[[gnu::const]]
Color
GetTranslucentInfoBoxColor(bool inverse,
                           InfoBoxSettings::Background t) noexcept;

/**
 * The colour of the caption bar of a translucent Shaded InfoBox.  A
 * translucent grey would be washed out by the map behind it, so the
 * bar is a light tint over the box instead: a little black on the
 * light theme, a little white on the dark one, which keeps the
 * contrast to the box the solid grey has.
 *
 * @param solid the caption colour of a solid box, returned where the
 * canvas cannot blend
 */
[[gnu::const]]
Color
GetTranslucentCaptionColor(bool inverse, Color solid) noexcept;

/**
 * Apply the translucency of the InfoBox background to a solid
 * colour.  Only OpenGL can blend; on the memory canvas, the colour
 * stays solid.
 */
[[gnu::const]]
Color
MakeTranslucent(Color color, InfoBoxSettings::Background t) noexcept;

/**
 * Draw a soft shadow around the box in the gap between it and the
 * window edge, so that a Floating box or a Dock panel appears to
 * float above the map.  Does nothing unless the shape is set back
 * from the window edge, and nothing on the memory canvas.  Call this
 * before painting the box.
 */
void
DrawInfoBoxHalo(Canvas &canvas, PixelRect rc,
                const InfoBoxBackgroundShape &shape) noexcept;

/**
 * Blur the map behind the box, see
 * #InfoBoxSettings::Background::FROSTED.  Call this before
 * #DrawInfoBoxBackground().  Does nothing on the memory canvas.
 */
void
DrawInfoBoxFrostedGlass(Canvas &canvas, PixelRect rc,
                        const InfoBoxBackgroundShape &shape) noexcept;

/**
 * Fill the box: the given rectangle inset by the shape, with rounded
 * corners where two of the shape's edges meet.  Translucent colours
 * are blended with what is behind, i.e. the map.  With the default
 * shape, this fills the whole rectangle.
 */
void
DrawInfoBoxBackground(Canvas &canvas, PixelRect rc,
                      const InfoBoxBackgroundShape &shape,
                      Color color) noexcept;

/**
 * The colour of the outline and the separators of a Dock panel: the
 * InfoBox border grey, but translucent where the canvas can blend,
 * so the lines do not weigh on the panel.
 */
[[gnu::const]]
Color
GetInfoBoxOutlineColor() noexcept;

/**
 * Draw separator lines along the given edges (BORDER* flags) of the
 * box, i.e. of the given rectangle inset by the shape, with the given
 * pen.
 */
void
DrawInfoBoxSeparators(Canvas &canvas, PixelRect rc,
                      const InfoBoxBackgroundShape &shape,
                      unsigned edges, const Pen &pen) noexcept;

/**
 * Draw the outline of the box along the shape's edges (and only
 * those, so adjacent boxes do not double their separator) with the
 * given pen.  Does nothing unless the shape asks for an outline.
 */
void
DrawInfoBoxOutline(Canvas &canvas, PixelRect rc,
                   const InfoBoxBackgroundShape &shape,
                   const Pen &pen) noexcept;
