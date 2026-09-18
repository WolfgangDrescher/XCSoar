// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class Canvas;
struct PixelRect;
struct BulkPixelPoint;

namespace OpenGL {

/**
 * Replace the contents of the framebuffer within the given polygon by
 * a blurred copy of themselves ("frosted glass").  Draw the map
 * first, then call this, then paint the translucent box on top.
 *
 * The blurred copy is made once per frame for the whole screen, at
 * the first call, and reused by all further calls in the same frame
 * (see #frame_serial): one framebuffer copy and three small
 * off-screen passes per frame, however many boxes there are.  It
 * shows what had been painted when the first box asked for it, so
 * paint everything that belongs behind the boxes before them.
 *
 * @param canvas the on-screen canvas the polygon belongs to
 * @param rc the bounding rectangle of the polygon, in canvas
 * coordinates
 * @param points the polygon, a triangle fan in canvas coordinates
 */
void
DrawFrostedGlass(Canvas &canvas, PixelRect rc,
                 const BulkPixelPoint *points, unsigned num_points) noexcept;

/**
 * Release the textures and frame buffers #DrawFrostedGlass() keeps
 * for reuse.  Called before the OpenGL context goes away.
 */
void
DeinitFrostedGlass() noexcept;

} // namespace OpenGL
