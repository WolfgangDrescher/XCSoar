// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GeoCircleRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Projection/GeoCircle.hpp"
#include "Projection/Projection.hpp"
#include "Geo/GeoPoint.hpp"

#include <cassert>
#include <iterator>

void
DrawGeoCircle(Canvas &canvas, const Projection &projection,
              const GeoPoint &center, double radius) noexcept
{
  if (!projection.HasScreenTilt()) {
    canvas.DrawCircle(projection.GeoToScreen(center),
                      projection.GeoToScreenDistance(radius));
    return;
  }

  BulkPixelPoint points[MAX_GEO_CIRCLE_SEGMENTS];
  const unsigned n = ProjectGeoCircle(projection, center, radius, points);
  assert(n <= std::size(points));
  canvas.DrawPolygon(points, n);
}

void
DrawGeoSegment(Canvas &canvas, const Projection &projection,
               const GeoPoint &center, double radius,
               Angle start, Angle end) noexcept
{
  if (!projection.HasScreenTilt()) {
    canvas.DrawSegment(projection.GeoToScreen(center),
                       projection.GeoToScreenDistance(radius),
                       start - projection.GetScreenAngle(),
                       end - projection.GetScreenAngle());
    return;
  }

  BulkPixelPoint points[MAX_GEO_ARC_POINTS + 1];
  points[0] = projection.GeoToScreen(center);
  const unsigned n = ProjectGeoArc(projection, center, radius, start, end,
                                   points + 1);
  assert(n + 1 <= std::size(points));
  canvas.DrawTriangleFan(points, n + 1);
}

void
DrawGeoAnnulus(Canvas &canvas, const Projection &projection,
               const GeoPoint &center, double inner_radius, double radius,
               Angle start, Angle end) noexcept
{
  if (!projection.HasScreenTilt()) {
    canvas.DrawAnnulus(projection.GeoToScreen(center),
                       projection.GeoToScreenDistance(inner_radius),
                       projection.GeoToScreenDistance(radius),
                       start - projection.GetScreenAngle(),
                       end - projection.GetScreenAngle());
    return;
  }

  const double sweep = CalcGeoArcSweep(start, end);
  const unsigned n = CalcGeoArcSegments(projection, radius, sweep);

  BulkPixelPoint points[2 * MAX_GEO_ARC_POINTS];
  ProjectGeoArcPoints(projection, center, radius, start, sweep / n, n + 1,
                      points);
  /* the inner arc runs backwards, so the outline stays simple */
  ProjectGeoArcPoints(projection, center, inner_radius, end, -sweep / n, n + 1,
                      points + n + 1);
  assert(2 * (n + 1) <= std::size(points));
  canvas.DrawPolygon(points, 2 * (n + 1));
}

void
DrawGeoKeyhole(Canvas &canvas, const Projection &projection,
               const GeoPoint &center, double inner_radius, double radius,
               Angle start, Angle end) noexcept
{
  if (!projection.HasScreenTilt()) {
    canvas.DrawKeyhole(projection.GeoToScreen(center),
                       projection.GeoToScreenDistance(inner_radius),
                       projection.GeoToScreenDistance(radius),
                       start - projection.GetScreenAngle(),
                       end - projection.GetScreenAngle());
    return;
  }

  const double sweep = CalcGeoArcSweep(start, end);
  const unsigned n = CalcGeoArcSegments(projection, radius, sweep);

  /* the sector, followed by the remainder of the inner circle */
  const double rest = 360 - sweep;
  const unsigned n_rest = CalcGeoArcSegments(projection, inner_radius, rest);

  BulkPixelPoint points[2 * MAX_GEO_ARC_POINTS];
  ProjectGeoArcPoints(projection, center, radius, start, sweep / n, n + 1,
                      points);
  ProjectGeoArcPoints(projection, center, inner_radius, end, rest / n_rest,
                      n_rest + 1, points + n + 1);
  assert(n + n_rest + 2 <= std::size(points));
  canvas.DrawPolygon(points, n + n_rest + 2);
}
