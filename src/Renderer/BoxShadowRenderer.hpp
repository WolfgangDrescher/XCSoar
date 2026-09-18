// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

struct PixelRect;

/**
 * Draw a soft black drop shadow around the given rectangle, to make it
 * look like it floats above what is painted behind it.  The shadow
 * surrounds the rectangle evenly on all four sides, and its corners
 * are rounded, the way the corners of a blurred rectangle are.
 *
 * This must be called before the box itself is painted: the shadow is
 * drawn as a solid shape which is blurred at its edges, so the area
 * covered by the box gets painted over as well.
 *
 * The rectangle is relative to the current Canvas, and the shadow
 * extends beyond it, which means the caller must be allowed to paint
 * outside of its own window.
 *
 * This is implemented with OpenGL and does nothing on other platforms.
 */
void
DrawBoxShadow(const PixelRect &rc) noexcept;

/**
 * Draw a soft shadow ("halo") around a box with rounded corners
 * which sits inside a larger area, e.g. an InfoBox set back from its
 * window edge.  Unlike #DrawBoxShadow(), nothing is painted inside
 * the box, so a translucent box is not darkened: the halo is a ring
 * whose inner contour follows the rounded box.  Nothing is painted
 * outside @p clip, so the halo stays inside the caller's window;
 * where the box reaches @p clip (or beyond), there is no halo on
 * that side.
 *
 * The blur is centered on the box edge: the halo has half of
 * @p alpha at the edge and fades out @p blur/2 pixels beyond it.
 *
 * This is implemented with OpenGL and does nothing on other platforms.
 *
 * @param clip the area the halo may be painted in, in canvas
 * coordinates
 * @param box the box which casts the halo
 * @param corner_radius the radius of the box's rounded corners
 * @param blur the width of the blurred transition, in pixels
 * @param alpha the opacity of the black halo where it is darkest
 */
void
DrawBoxHalo(const PixelRect &clip, const PixelRect &box,
            unsigned corner_radius, unsigned blur, uint8_t alpha) noexcept;
