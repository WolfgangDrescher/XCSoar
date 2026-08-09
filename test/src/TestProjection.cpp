// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Projection/Projection.hpp"
#include "TestUtil.hpp"

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

int main()
{
  plan_tests(17);

  test_simple();
  test_tilt();

  return exit_status();
}
