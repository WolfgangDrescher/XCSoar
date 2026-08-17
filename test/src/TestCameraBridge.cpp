// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapWindow/MapLibre/CameraBridge.hpp"
#include "Projection/WindowProjection.hpp"
#include "TestUtil.hpp"

#include <cmath>

/**
 * Build a #WindowProjection like the MapWindow does: a geographic
 * location mapped to a screen origin, a scale in px/m and a screen
 * rotation angle.
 */
static WindowProjection
MakeProjection(const GeoPoint &location, PixelSize screen_size,
               PixelPoint screen_origin, double scale_px_per_m,
               Angle screen_angle) noexcept
{
  WindowProjection projection;
  projection.SetScreenSize(screen_size);
  projection.SetGeoLocation(location);
  projection.SetScreenOrigin(screen_origin);
  projection.SetScale(scale_px_per_m);
  projection.SetScreenAngle(screen_angle);
  return projection;
}

static void
TestBasicProperties() noexcept
{
  const GeoPoint location(Angle::Degrees(8.5), Angle::Degrees(47));
  const auto projection = MakeProjection(location, {800, 600}, {400, 300},
                                         0.008, Angle::Degrees(30));

  const auto camera = MapLibre::CameraFromProjection(projection);

  /* the bearing is the screen angle */
  ok1(equals(camera.bearing.Degrees(), 30));

  /* the screen origin is the viewport center, therefore the camera
     center is the projection's geo location */
  ok1(equals(camera.center.longitude.Degrees(), location.longitude.Degrees()));
  ok1(equals(camera.center.latitude.Degrees(), location.latitude.Degrees()));

  /* the camera center must land in the middle of the viewport */
  const auto center_px = camera.GeoToScreen(camera.center);
  ok1(is_zero(center_px.x - 400));
  ok1(is_zero(center_px.y - 300));

  /* the zoom must reproduce XCSoar's px/m scale at the center: one
     degree of longitude at the center latitude must map to the same
     number of pixels in both projections */
  const double world_size = camera.WorldSizePixels();
  const double maplibre_px_per_lon_degree = world_size / 360;
  const double flat_earth_px_per_lon_degree = camera.center.latitude.cos()
    * Angle::Degrees(1).Radians() * 6371000 * projection.GetScale();
  ok1(equals(maplibre_px_per_lon_degree, flat_earth_px_per_lon_degree, 1000));

  /* a negative screen angle is normalised to [0, 360) */
  const auto projection2 = MakeProjection(location, {800, 600}, {400, 300},
                                          0.008, Angle::Degrees(-90));
  const auto camera2 = MapLibre::CameraFromProjection(projection2);
  ok1(equals(camera2.bearing.Degrees(), 270));
}

/**
 * Compare the screen position of geo points projected by XCSoar's
 * flat-earth projection and by the Web Mercator camera reference
 * implementation.  The divergence grows with the distance from the
 * viewport center; this test documents its bounds.
 *
 * @return the number of "ok" checks performed
 */
static void
TestAlignment(const GeoPoint &location, PixelSize screen_size,
              PixelPoint screen_origin, double scale_px_per_m,
              Angle screen_angle,
              double distance_m, double tolerance_px) noexcept
{
  const auto projection = MakeProjection(location, screen_size, screen_origin,
                                         scale_px_per_m, screen_angle);
  const auto camera = MapLibre::CameraFromProjection(projection);

  /* geo points around the projection's geo location, constructed via
     simple equirectangular offsets (both projections receive the same
     input, so the exact geodesic shape does not matter here) */
  const double earth_radius = 6371000;
  const Angle dlat = Angle::Radians(distance_m / earth_radius);
  const Angle dlon = Angle::Radians(distance_m / earth_radius)
    / location.latitude.cos();

  const GeoPoint points[] = {
    {location.longitude, location.latitude + dlat},         // north
    {location.longitude + dlon, location.latitude + dlat},  // north-east
    {location.longitude + dlon, location.latitude},         // east
    {location.longitude + dlon, location.latitude - dlat},  // south-east
    {location.longitude, location.latitude - dlat},         // south
    {location.longitude - dlon, location.latitude - dlat},  // south-west
    {location.longitude - dlon, location.latitude},         // west
    {location.longitude - dlon, location.latitude + dlat},  // north-west
  };

  for (const auto &p : points) {
    const auto flat = projection.GeoToScreen(p);
    const auto mercator = camera.GeoToScreen(p);

    const double error = std::hypot(mercator.x - flat.x, mercator.y - flat.y);
    ok1(error <= tolerance_px);
  }
}

int main()
{
  plan_tests(7 + 8 * 8);

  TestBasicProperties();

  const GeoPoint alps(Angle::Degrees(8.5), Angle::Degrees(47));
  const GeoPoint north(Angle::Degrees(18), Angle::Degrees(60));

  /* 100 km wide viewport (cruise): sub-pixel to low single-digit
     pixel divergence, dominated by integer rounding near the
     center */
  TestAlignment(alps, {800, 600}, {400, 300}, 0.008, Angle::Zero(),
                5000, 2);
  TestAlignment(alps, {800, 600}, {400, 300}, 0.008, Angle::Zero(),
                25000, 4);
  TestAlignment(alps, {800, 600}, {400, 300}, 0.008, Angle::Zero(),
                50000, 8);

  /* rotated (track-up) with the aircraft offset below the center */
  TestAlignment(alps, {800, 600}, {400, 450}, 0.008, Angle::Degrees(137),
                25000, 5);

  /* zoomed in (8 km viewport, circling) */
  TestAlignment(alps, {800, 600}, {400, 300}, 0.1, Angle::Degrees(200),
                2000, 2);

  /* high latitude */
  TestAlignment(north, {800, 600}, {400, 300}, 0.008, Angle::Zero(),
                25000, 6);

  /* extreme zoom-out (500 km viewport): this is where flat-earth and
     Web Mercator visibly diverge; the tolerance documents the
     expected error at the viewport edge */
  TestAlignment(alps, {800, 600}, {400, 300}, 0.0016, Angle::Zero(),
                200000, 25);

  /* offset origin, north-up, medium zoom */
  TestAlignment(alps, {640, 480}, {320, 360}, 0.02, Angle::Zero(),
                10000, 3);

  return exit_status();
}
