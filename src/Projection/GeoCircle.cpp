// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GeoCircle.hpp"
#include "Projection.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Geo/GeoPoint.hpp"
#include "Geo/Math.hpp"
#include "Math/Constants.hpp"

#include <algorithm>
#include <cmath>

/**
 * How many segments are needed to make the approximation of a full
 * circle indistinguishable from the real thing?
 *
 * An n-gon deviates from its circumscribed circle by about
 * r*pi^2/(2n^2), so n=pi*sqrt(r) keeps the error below half a pixel.
 * A tilted projection magnifies the near half of the circle, which is
 * covered by a safety factor on the radius.
 */
[[gnu::pure]]
static unsigned
CalcCircleSegments(const Projection &projection, double radius) noexcept
{
  const double r = projection.DistanceMetersToPixels(radius);
  const auto n = (unsigned)std::ceil(M_PI * std::sqrt(std::max(2 * r, 1.)));
  return std::clamp(n, 16u, MAX_GEO_CIRCLE_SEGMENTS);
}

double
CalcGeoArcSweep(Angle start, Angle end) noexcept
{
  const double sweep = (end - start).AsBearing().Degrees();

  /* coincident radials mean a full circle; the epsilon keeps rounding
     noise in the radials from collapsing the arc to nothing */
  return sweep > 1e-6 ? sweep : 360;
}

unsigned
CalcGeoArcSegments(const Projection &projection, double radius,
                   double sweep) noexcept
{
  const unsigned full = CalcCircleSegments(projection, radius);
  const auto n = (unsigned)std::ceil(full * std::fabs(sweep) / 360.);
  return std::max(2u, n);
}

void
ProjectGeoArcPoints(const Projection &projection, const GeoPoint &center,
                    double radius, Angle start, double step, unsigned count,
                    BulkPixelPoint *points) noexcept
{
  for (unsigned i = 0; i < count; ++i) {
    const Angle bearing = start + Angle::Degrees(step * i);
    points[i] = projection.GeoToScreen(FindLatitudeLongitude(center, bearing,
                                                             radius));
  }
}

unsigned
ProjectGeoCircle(const Projection &projection, const GeoPoint &center,
                 double radius, BulkPixelPoint *points) noexcept
{
  const unsigned n = CalcCircleSegments(projection, radius);

  ProjectGeoArcPoints(projection, center, radius, Angle::Zero(), 360. / n, n,
                      points);
  return n;
}

unsigned
ProjectGeoArc(const Projection &projection, const GeoPoint &center,
              double radius, Angle start, Angle end,
              BulkPixelPoint *points) noexcept
{
  const double sweep = CalcGeoArcSweep(start, end);
  const unsigned n = CalcGeoArcSegments(projection, radius, sweep);

  ProjectGeoArcPoints(projection, center, radius, start, sweep / n, n + 1,
                      points);
  return n + 1;
}
