// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Projection/Projection.hpp"
#include "Projection/GeoCircle.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Geo/Math.hpp"
#include "TestUtil.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

static void
TestGeoScreenCouple(const Projection prj, const GeoPoint geo,
                    int x, int y)
{
  auto tmp_pt = prj.GeoToScreen(geo);
  ok1(tmp_pt.x == x);
  ok1(tmp_pt.y == y);

  GeoPoint tmp_geo = prj.ScreenToGeo({x, y});
  ok1(equals(tmp_geo.latitude, geo.latitude));
  ok1(equals(tmp_geo.longitude, geo.longitude));
}

static void
test_simple()
{
  Projection prj;
  prj.SetGeoLocation(GeoPoint::Zero());

  TestGeoScreenCouple(prj, GeoPoint(Angle::Zero(),
                                    Angle::Zero()), 0, 0);
}

static void
test_tilt()
{
  Projection prj;
  prj.SetGeoLocation(GeoPoint::Zero());
  prj.SetScale(1);
  prj.SetScreenOrigin(0, 300);
  prj.SetScreenAngle(Angle::Degrees(30));
  prj.SetScreenTilt(Angle::Degrees(45), 1000);

  ok1(prj.HasScreenTilt());

  /* the ScreenOrigin is invariant under the tilt */
  const auto origin = prj.GeoToScreen(GeoPoint::Zero());
  ok1(origin.x == 0);
  ok1(origin.y == 300);

  /* the conversions must be inverses of each other */
  static constexpr PixelPoint round_trip_points[] = {
    {100, 100}, {-150, 250}, {50, 500},
  };

  for (const auto &p : round_trip_points) {
    const GeoPoint geo = prj.ScreenToGeo(p);
    const auto q = prj.GeoToScreen(geo);
    ok1(abs(q.x - p.x) <= 3);
    ok1(abs(q.y - p.y) <= 3);
  }

  Projection flat = prj;
  flat.SetScreenTilt(Angle::Zero(), 0);
  ok1(!flat.HasScreenTilt());

  /* a point which the top-down view shows 200px above the
     ScreenOrigin appears compressed towards the horizon in the
     tilted view */
  const GeoPoint ahead = flat.ScreenToGeo({0, 100});
  const auto tilted_ahead = prj.GeoToScreen(ahead);
  ok1(abs(tilted_ahead.x) <= 3);
  ok1(tilted_ahead.y > 120 && tilted_ahead.y < 300);

  /* while a point below the ScreenOrigin is magnified */
  const GeoPoint behind = flat.ScreenToGeo({0, 500});
  const auto tilted_behind = prj.GeoToScreen(behind);
  ok1(tilted_behind.y > 400 && tilted_behind.y < 500);
}

/**
 * Builds a projection with a 3000 m radius fitting comfortably on a
 * 400x600 screen.
 */
static Projection
MakeCircleProjection()
{
  Projection prj;
  prj.SetGeoLocation(GeoPoint(Angle::Degrees(8), Angle::Degrees(47)));
  prj.SetScale(0.05);
  prj.SetScreenOrigin(200, 300);
  return prj;
}

static void
test_geo_circle()
{
  constexpr double radius = 3000;

  const Projection prj = MakeCircleProjection();

  BulkPixelPoint points[MAX_GEO_CIRCLE_SEGMENTS];
  const unsigned n = ProjectGeoCircle(prj, prj.GetGeoLocation(), radius,
                                      points);
  ok1(n >= 16 && n <= MAX_GEO_CIRCLE_SEGMENTS);

  /* without tilt every point sits on a circle of the projected
     radius */
  const auto expected = (double)prj.GeoToScreenDistance(radius);
  double min_r = 1e9, max_r = 0;
  for (unsigned i = 0; i < n; ++i) {
    const double dx = points[i].x - 200, dy = points[i].y - 300;
    const double r = std::hypot(dx, dy);
    min_r = std::min(min_r, r);
    max_r = std::max(max_r, r);
  }

  ok1(min_r > expected - 2 && max_r < expected + 2);

  /* the same circle seen from a tilted viewpoint is compressed
     vertically, but keeps its width */
  Projection tilted = prj;
  tilted.SetScreenTilt(Angle::Degrees(50), 900);

  BulkPixelPoint tp[MAX_GEO_CIRCLE_SEGMENTS];
  const unsigned tn = ProjectGeoCircle(tilted, tilted.GetGeoLocation(), radius,
                                       tp);

  int min_x = 99999, max_x = -99999, min_y = 99999, max_y = -99999;
  for (unsigned i = 0; i < tn; ++i) {
    min_x = std::min(min_x, (int)tp[i].x);
    max_x = std::max(max_x, (int)tp[i].x);
    min_y = std::min(min_y, (int)tp[i].y);
    max_y = std::max(max_y, (int)tp[i].y);
  }

  const int width = max_x - min_x, height = max_y - min_y;
  ok1(height < width);

  /* the far (upper) half is compressed more than the near half, so
     the center of the outline sits below the projected center */
  const int center_y = tilted.GeoToScreen(tilted.GetGeoLocation()).y;
  ok1((min_y + max_y) / 2 > center_y);
}

static void
test_geo_arc()
{
  constexpr double radius = 3000;

  Projection prj = MakeCircleProjection();
  prj.SetScreenTilt(Angle::Degrees(45), 900);

  const GeoPoint center = prj.GetGeoLocation();
  const Angle start = Angle::Degrees(30), end = Angle::Degrees(150);

  BulkPixelPoint points[MAX_GEO_ARC_POINTS];
  const unsigned n = ProjectGeoArc(prj, center, radius, start, end, points);
  ok1(n >= 3 && n <= MAX_GEO_ARC_POINTS);

  /* the arc runs clockwise and its ends match the radials exactly */
  ok1(std::fabs(CalcGeoArcSweep(start, end) - 120) < 1e-6);

  const auto p_start = prj.GeoToScreen(FindLatitudeLongitude(center, start,
                                                             radius));
  const auto p_end = prj.GeoToScreen(FindLatitudeLongitude(center, end,
                                                           radius));
  ok1(points[0] == p_start);
  ok1(points[n - 1] == p_end);

  /* a degenerate arc covers the whole circle */
  ok1(CalcGeoArcSweep(start, start) == 360);
}

int main()
{
  plan_tests(26);

  test_simple();
  test_tilt();
  test_geo_circle();
  test_geo_arc();

  return exit_status();
}
