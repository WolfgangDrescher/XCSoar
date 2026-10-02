// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/dim/Point.hpp"

class Canvas;
struct PixelRect;
struct BulkPixelPoint;

namespace OpenGL {

/**
 * How far the map has moved since a drag or zoom began, see
 * HoldFrostedGlass(): what was at point q then is now at
 * center + zoom * (q - center) + offset.
 */
struct FrostedGlassMotion {
  /** the fixed point of the zoom, in frame buffer coordinates */
  PixelPoint center;

  /** how far the map content has moved, in pixels */
  PixelPoint offset;

  /** how much it has grown: 2 after zooming in by a factor of two */
  float zoom;
};

/**
 * Replace the contents of the framebuffer within the given polygon by
 * a blurred copy of themselves ("frosted glass").  Draw the map
 * first, then call this, then paint the translucent box on top.
 *
 * The blurred copy is made for the whole frame buffer at the first
 * call of a frame and reused by all further calls in the same frame
 * (see #frame_serial) into the same frame buffer: one copy and three
 * small off-screen passes, however many shapes there are.  It is
 * even kept from one frame to the next while the map below has not
 * changed (see FrostedGlassMapPainted()) or is only being dragged
 * (see HoldFrostedGlass()).  It shows what had been painted when the
 * first shape asked for it, so paint everything that belongs behind
 * the shapes before them.  This works on the screen and inside a
 * #BufferCanvas (e.g. the map's), each with a copy of its own.
 *
 * @param canvas the canvas the polygon belongs to
 * @param rc the bounding rectangle of the polygon, in canvas
 * coordinates
 * @param points the polygon in canvas coordinates
 * @param convex is the polygon convex?  Then it is drawn as a
 * triangle fan; a concave one needs to be triangulated first
 */
void
DrawFrostedGlass(Canvas &canvas, PixelRect rc,
                 const BulkPixelPoint *points, unsigned num_points,
                 bool convex) noexcept;

/**
 * Tell the frosted glass that the map has been painted onto the
 * frame buffer in this frame.  While @p generation (which changes
 * whenever the map is rendered again) and @p origin (where the map
 * is) stay the same from one frame to the next, the blurred copies
 * of the screen are reused instead of being made again, which saves
 * the copy in all the frames which only update the InfoBoxes and
 * gauges over an unchanged map.
 */
void
FrostedGlassMapPainted(unsigned generation, PixelPoint origin) noexcept;

/**
 * Make the blurred copy for this frame and frame buffer now, if it
 * has not been made yet.  #DrawFrostedGlass() does this on its own;
 * call it explicitly before changing OpenGL state the off-screen
 * passes must not inherit, e.g. the stencil test.
 */
void
PrepareFrostedGlass(Canvas &canvas) noexcept;

/**
 * While the map is being dragged or zoomed, keep the blurred copies
 * instead of making new ones each frame, which would cost a good
 * part of each frame, and draw them moved along with the map as
 * described by @p motion.  Only when the map has moved far from the
 * copy is a new one made, as the stretched edges of the copy would
 * show.  Call this before painting the translucent shapes of a
 * frame; a null pointer ends the hold, and the next frame makes
 * fresh copies.
 */
void
HoldFrostedGlass(const FrostedGlassMotion *motion) noexcept;

/**
 * Release the textures and frame buffers #DrawFrostedGlass() keeps
 * for reuse.  Called before the OpenGL context goes away.
 */
void
DeinitFrostedGlass() noexcept;

} // namespace OpenGL
