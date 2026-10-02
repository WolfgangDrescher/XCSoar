// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/**
 * @file
 * @brief Small canvas helper functions
 */

#pragma once

class Canvas;
class Angle;
struct PixelPoint;
struct PixelRect;
struct BulkPixelPoint;

bool
Segment(Canvas &canvas, PixelPoint center, unsigned radius,
        Angle start, Angle end, bool horizon=false) noexcept;

bool
Annulus(Canvas &canvas, PixelPoint center, unsigned radius,
        Angle start, Angle end, unsigned inner_radius) noexcept;

bool
KeyHole(Canvas &canvas, PixelPoint center, unsigned radius,
        Angle start, Angle end, unsigned inner_radius) noexcept;

/**
 * Corner flags for RoundRect().
 */
enum RoundRectCorner : unsigned {
  ROUND_TOP_LEFT = 0x1,
  ROUND_TOP_RIGHT = 0x2,
  ROUND_BOTTOM_RIGHT = 0x4,
  ROUND_BOTTOM_LEFT = 0x8,
  ROUND_ALL_CORNERS = 0xf,
};

/**
 * The maximum number of points RoundRectPoints() generates.
 */
static constexpr unsigned ROUND_RECT_MAX_POINTS = (64 + 2) * 4;

/**
 * Generate the polygon (a triangle fan) of a rectangle whose selected
 * corners are rounded, as filled by RoundRect().
 *
 * @param pt room for at least #ROUND_RECT_MAX_POINTS points
 * @return the number of points written
 */
unsigned
RoundRectPoints(BulkPixelPoint *pt, PixelRect r, unsigned radius,
                unsigned corners) noexcept;

/**
 * Fill a rectangle whose selected corners are rounded.
 *
 * @param corners a bit mask of #RoundRectCorner; the other corners
 * stay square
 */
void
RoundRect(Canvas &canvas, PixelRect r, unsigned radius,
          unsigned corners=ROUND_ALL_CORNERS) noexcept;

/**
 * Edge flags for RoundRectOutline().
 */
enum RoundRectEdge : unsigned {
  ROUND_RECT_TOP = 0x1,
  ROUND_RECT_RIGHT = 0x2,
  ROUND_RECT_BOTTOM = 0x4,
  ROUND_RECT_LEFT = 0x8,
};

/**
 * Draw the outline of the shape RoundRect() fills, but only along
 * the selected edges, with the currently selected pen.  A rounded
 * corner belongs to both edges it joins.
 *
 * @param corners a bit mask of #RoundRectCorner
 * @param edges a bit mask of #RoundRectEdge
 */
void
RoundRectOutline(Canvas &canvas, PixelRect r, unsigned radius,
                 unsigned corners, unsigned edges) noexcept;

bool
Arc(Canvas &canvas, PixelPoint center, unsigned radius,
    Angle start, Angle end) noexcept;
