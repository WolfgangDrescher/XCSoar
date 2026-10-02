// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Util.hpp"
#include "Canvas.hpp"
#include "util/Macros.hpp"
#include "Math/Angle.hpp"

static constexpr unsigned CIRCLE_SEGS = 64;

static_assert(ROUND_RECT_MAX_POINTS == (CIRCLE_SEGS + 2) * 4);

[[gnu::const]]
static PixelPoint
CirclePoint(int radius, unsigned angle) noexcept
{
  assert(angle < ISINETABLE.size());

  return PixelPoint(ISINETABLE[angle] * radius / 1024,
                    -ISINETABLE[(angle + INT_QUARTER_CIRCLE) & INT_ANGLE_MASK] * radius / 1024);
}

[[gnu::const]]
static PixelPoint
CirclePoint(PixelPoint p, int radius, unsigned angle) noexcept
{
  return p + CirclePoint(radius, angle);
}

static void
segment_poly(BulkPixelPoint *pt, const PixelPoint center,
             const int radius, const unsigned istart, const unsigned iend,
             unsigned &npoly, const bool forward=true) noexcept
{
  assert(istart < ISINETABLE.size());
  assert(iend < ISINETABLE.size());

  // add start node
  pt[npoly++] = CirclePoint(center, radius, istart);

  // add intermediate nodes (if any)
  if (forward) {
    const unsigned ilast = istart < iend ? iend : iend + INT_ANGLE_RANGE;
    for (unsigned i = istart + INT_ANGLE_RANGE / CIRCLE_SEGS; i < ilast;
         i += INT_ANGLE_RANGE / CIRCLE_SEGS) {
      const unsigned angle = i & INT_ANGLE_MASK;
      pt[npoly] = CirclePoint(center, radius, angle);

      if (pt[npoly].x != pt[npoly-1].x || pt[npoly].y != pt[npoly-1].y)
        npoly++;
    }
  } else {
    const unsigned ilast = istart > iend ? iend + INT_ANGLE_RANGE: iend;
    for (unsigned i = istart + INT_ANGLE_RANGE / CIRCLE_SEGS + INT_ANGLE_RANGE; i > ilast;
         i -= INT_ANGLE_RANGE / CIRCLE_SEGS) {
      const unsigned angle = i & INT_ANGLE_MASK;
      pt[npoly] = CirclePoint(center, radius, angle);

      if (pt[npoly].x != pt[npoly-1].x || pt[npoly].y != pt[npoly-1].y)
        npoly++;
    }
  }

  // and end node
  pt[npoly++] = CirclePoint(center, radius, iend);
}

[[gnu::pure]]
static bool
IsCircleVisible(const Canvas &canvas,
                PixelPoint center, unsigned radius) noexcept
{
  return int(center.x + radius) >= 0 && center.x < int(canvas.GetWidth() + radius) &&
    int(center.y + radius) >= 0 && center.y < int(canvas.GetHeight() + radius);
}

bool
Segment(Canvas &canvas, PixelPoint center, unsigned radius,
        Angle start, Angle end, bool horizon) noexcept
{
  // dont draw if out of view
  if (!IsCircleVisible(canvas, center, radius))
    return false;

  const int istart = NATIVE_TO_INT(start.Native());
  const int iend = NATIVE_TO_INT(end.Native());

  unsigned npoly = 0;
  BulkPixelPoint pt[CIRCLE_SEGS+3];

  // add center point
  if (!horizon) {
    pt[0] = center;
    npoly = 1;
  }

  segment_poly(pt, center, radius, istart, iend, npoly);

  assert(npoly <= ARRAY_SIZE(pt));
  if (npoly)
    canvas.DrawTriangleFan(pt, npoly);

  return true;
}

bool
Annulus(Canvas &canvas, PixelPoint center, unsigned radius,
        Angle start, Angle end, unsigned inner_radius) noexcept
{
  // dont draw if out of view
  if (!IsCircleVisible(canvas, center, radius))
    return false;

  const int istart = NATIVE_TO_INT(start.Native());
  const int iend = NATIVE_TO_INT(end.Native());

  unsigned npoly = 0;
  BulkPixelPoint pt[(CIRCLE_SEGS+2)*2];

  segment_poly(pt, center, radius, istart, iend, npoly);
  segment_poly(pt, center, inner_radius, iend, istart, npoly, false);

  assert(npoly <= ARRAY_SIZE(pt));
  if (npoly)
    canvas.DrawPolygon(pt, npoly);

  return true;
}

bool
KeyHole(Canvas &canvas, PixelPoint center, unsigned radius,
        Angle start, Angle end, unsigned inner_radius) noexcept
{
  // dont draw if out of view
  if (!IsCircleVisible(canvas, center, radius))
    return false;

  const int istart = NATIVE_TO_INT(start.Native());
  const int iend = NATIVE_TO_INT(end.Native());

  unsigned npoly = 0;
  BulkPixelPoint pt[(CIRCLE_SEGS+2)*2];

  segment_poly(pt, center, radius, istart, iend, npoly);
  segment_poly(pt, center, inner_radius, iend, istart, npoly);

  assert(npoly <= ARRAY_SIZE(pt));
  if (npoly)
    canvas.DrawPolygon(pt, npoly);

  return true;
}

/**
 * Append the points of one corner of a rounded rectangle: the arc if
 * the corner is rounded, or just the corner point.
 */
static void
RoundRectCornerPoints(BulkPixelPoint *pt, unsigned &npoly,
                      const PixelRect &r, unsigned radius,
                      RoundRectCorner corner, bool rounded) noexcept
{
  switch (corner) {
  case ROUND_TOP_LEFT:
    if (rounded)
      segment_poly(pt, r.GetTopLeft().At(radius, radius), radius,
                   INT_ANGLE_RANGE * 3 / 4,
                   INT_ANGLE_RANGE - 1,
                   npoly);
    else
      pt[npoly++] = r.GetTopLeft();
    break;

  case ROUND_TOP_RIGHT:
    if (rounded)
      segment_poly(pt, r.GetTopRight().At(-(int)radius, radius), radius,
                   0, INT_ANGLE_RANGE / 4 - 1,
                   npoly);
    else
      pt[npoly++] = r.GetTopRight();
    break;

  case ROUND_BOTTOM_RIGHT:
    if (rounded)
      segment_poly(pt, r.GetBottomRight().At(-(int)radius, -(int)radius),
                   radius,
                   INT_ANGLE_RANGE / 4,
                   INT_ANGLE_RANGE / 2 - 1,
                   npoly);
    else
      pt[npoly++] = r.GetBottomRight();
    break;

  case ROUND_BOTTOM_LEFT:
    if (rounded)
      segment_poly(pt, r.GetBottomLeft().At(radius, -(int)radius), radius,
                   INT_ANGLE_RANGE / 2,
                   INT_ANGLE_RANGE * 3 / 4 - 1,
                   npoly);
    else
      pt[npoly++] = r.GetBottomLeft();
    break;

  case ROUND_ALL_CORNERS:
    break;
  }
}

/**
 * The corners in clockwise order, starting at the top left.
 */
static constexpr RoundRectCorner round_rect_corners[] = {
  ROUND_TOP_LEFT, ROUND_TOP_RIGHT, ROUND_BOTTOM_RIGHT, ROUND_BOTTOM_LEFT,
};

unsigned
RoundRectPoints(BulkPixelPoint *pt, PixelRect r, unsigned radius,
                unsigned corners) noexcept
{
  unsigned npoly = 0;

  for (const RoundRectCorner corner : round_rect_corners)
    RoundRectCornerPoints(pt, npoly, r, radius, corner, corners & corner);

  assert(npoly <= ROUND_RECT_MAX_POINTS);
  return npoly;
}

void
RoundRect(Canvas &canvas, PixelRect r, unsigned radius,
          unsigned corners) noexcept
{
  BulkPixelPoint pt[ROUND_RECT_MAX_POINTS];
  const unsigned npoly = RoundRectPoints(pt, r, radius, corners);

  if (npoly)
    canvas.DrawTriangleFan(pt, npoly);
}

void
RoundRectOutline(Canvas &canvas, PixelRect r, unsigned radius,
                 unsigned corners, unsigned edges) noexcept
{
  /* the edges in clockwise order, starting at the top; each runs from
     round_rect_corners[i] to round_rect_corners[i + 1] */
  static constexpr RoundRectEdge round_rect_edges[] = {
    ROUND_RECT_TOP, ROUND_RECT_RIGHT, ROUND_RECT_BOTTOM, ROUND_RECT_LEFT,
  };

  BulkPixelPoint pt[(CIRCLE_SEGS+2)*2];

  for (unsigned i = 0; i < 4; ++i) {
    if ((edges & round_rect_edges[i]) == 0)
      continue;

    const RoundRectCorner start = round_rect_corners[i];
    const RoundRectCorner end = round_rect_corners[(i + 1) % 4];

    unsigned npoly = 0;
    RoundRectCornerPoints(pt, npoly, r, radius, start, corners & start);
    RoundRectCornerPoints(pt, npoly, r, radius, end, corners & end);

    assert(npoly <= ARRAY_SIZE(pt));
    if (npoly >= 2)
      canvas.DrawPolyline(pt, npoly);
  }
}

bool
Arc(Canvas &canvas, PixelPoint center, unsigned radius,
    Angle start, Angle end) noexcept
{
  // dont draw if out of view
  if (!IsCircleVisible(canvas, center, radius))
    return false;

  const int istart = NATIVE_TO_INT(start.Native());
  const int iend = NATIVE_TO_INT(end.Native());

  unsigned npoly = 0;
  BulkPixelPoint pt[CIRCLE_SEGS+3];

  segment_poly(pt, center, radius, istart, iend, npoly);

  assert(npoly <= ARRAY_SIZE(pt));
  if (npoly)
    canvas.DrawPolyline(pt, npoly);

  return true;
}
