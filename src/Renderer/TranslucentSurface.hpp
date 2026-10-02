// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Util.hpp" // for ROUND_ALL_CORNERS
#include "ui/dim/Point.hpp"

#include <cstdint>

struct PixelRect;
struct BulkPixelPoint;
class Canvas;
class Color;

/**
 * Translucent surfaces over the map: InfoBoxes, labels, gauges, bars.
 * They share three levels of translucency, and with "frosted glass"
 * enabled (UISettings::frosted_glass), the map is blurred behind
 * them before they are filled, which keeps their contents readable
 * over busy terrain.  Without it, the Frost*() functions do nothing,
 * and so they do on the memory canvas, which cannot blend; there,
 * Translucent() returns the solid colour.
 *
 * Shadows and halos (BoxShadowRenderer) are no surfaces and keep
 * their own alpha values.
 */
namespace TranslucentSurface {

/**
 * How much of the map shows through a surface.  The exact alpha
 * depends on the frosted glass setting, see Alpha(): the blur keeps
 * the contents readable, so a frosted surface may let more of the
 * map show than a clear one.
 */
enum class Translucency : uint8_t {
  /**
   * Nearly solid, the map only shimmers through: InfoBoxes, pills
   * (94 percent opaque, frosted 88 percent).
   */
  SUBTLE,

  /** gauges (82 percent opaque, frosted 69 percent) */
  MODERATE,

  /**
   * Mostly map: labels, bars, the gesture trail (63 percent opaque,
   * frosted 50 percent).
   */
  STRONG,
};

/**
 * Enable or disable the frosted glass effect.  Called when the look
 * is (re)initialised from the settings.
 */
void
SetFrosted(bool enabled) noexcept;

[[gnu::pure]]
bool
IsFrosted() noexcept;

/**
 * The alpha value for the given translucency, depending on the
 * frosted glass setting.
 */
[[gnu::pure]]
uint8_t
Alpha(Translucency translucency) noexcept;

/**
 * The colour with the alpha of the given translucency; on the memory
 * canvas, the solid colour.
 */
[[gnu::pure]]
Color
Translucent(Color color, Translucency translucency) noexcept;

/**
 * Make the blurred copy of the map for this frame now; see
 * OpenGL::PrepareFrostedGlass().  Only needed before changing OpenGL
 * state the blur must not inherit, e.g. the stencil test.
 */
void
Prepare(Canvas &canvas) noexcept;

/**
 * Blur the map behind the given polygon (in canvas coordinates).
 * Where several translucent polygons overlap, blur behind all of
 * them before filling the first, or the later blur erases the
 * earlier fill.
 *
 * @param convex is the polygon convex?  A concave one costs a
 * triangulation
 */
void
Frost(Canvas &canvas, const BulkPixelPoint *points,
      unsigned num_points, bool convex=false) noexcept;

/**
 * Blur the map behind a rectangle.  With a stencil test active, only
 * the pixels it lets through are blurred.
 */
void
FrostRect(Canvas &canvas, PixelRect rc) noexcept;

/**
 * Blur the map behind a rectangle with rounded corners, as filled by
 * RoundRect().
 */
void
FrostRoundRect(Canvas &canvas, PixelRect rc, unsigned radius,
               unsigned corners=ROUND_ALL_CORNERS) noexcept;

/**
 * Blur the map behind a circle.
 */
void
FrostCircle(Canvas &canvas, PixelPoint center, unsigned radius) noexcept;

/**
 * Blur the map behind a rectangle with rounded corners and fill it
 * with the colour at the given translucency, outlined with the
 * selected pen.
 */
void
FillRoundRect(Canvas &canvas, PixelRect rc, unsigned radius,
              Color color, Translucency translucency,
              unsigned corners=ROUND_ALL_CORNERS) noexcept;

/**
 * Blur the map behind a circle and fill it with the colour at the
 * given translucency, outlined with the selected pen.
 */
void
FillCircle(Canvas &canvas, PixelPoint center, unsigned radius,
           Color color, Translucency translucency) noexcept;

/**
 * How far the map has moved since a drag or zoom began: what was at
 * point q then is now at center + zoom * (q - center) + offset.
 */
struct MapMotion {
  /** the fixed point of the zoom, in frame buffer coordinates */
  PixelPoint center;

  /** how far the map content has moved, in pixels */
  PixelPoint offset;

  /** how much it has grown: 2 after zooming in by a factor of two */
  float zoom;
};

/**
 * While the map is being dragged or zoomed, let the surfaces reuse
 * their blurred copy of the map, moved along with the map, instead
 * of making a new one each frame; see OpenGL::HoldFrostedGlass().
 * Call it before painting the surfaces of a frame.
 */
void
Hold(const MapMotion &motion) noexcept;

/**
 * End the Hold(): the next frame makes fresh copies.
 */
void
Release() noexcept;

/**
 * The map has been painted onto the screen in this frame; while
 * @p generation (changed whenever the map is rendered again) and
 * @p origin (where the map is) stay the same, the blur is reused
 * from one frame to the next.  See OpenGL::FrostedGlassMapPainted().
 */
void
MapPainted(unsigned generation, PixelPoint origin) noexcept;

} // namespace TranslucentSurface
