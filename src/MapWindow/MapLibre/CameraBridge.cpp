// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CameraBridge.hpp"
#include "Projection/WindowProjection.hpp"
#include "Geo/FAISphere.hpp"
#include "Math/Constants.hpp"

#include <algorithm>
#include <cmath>

namespace MapLibre {

/** the Web Mercator latitude limit */
static constexpr Angle MAX_MERCATOR_LATITUDE = Angle::Degrees(85.051129);

/**
 * Convert a #GeoPoint to Web Mercator world coordinates in the range
 * [0, 1) (x growing east, y growing south).
 */
[[gnu::pure]]
static DoublePoint2D
GeoToWorld(const GeoPoint &p) noexcept
{
  const Angle latitude = std::clamp(p.latitude,
                                    -MAX_MERCATOR_LATITUDE,
                                    MAX_MERCATOR_LATITUDE);

  const double x = (p.longitude.AsDelta().Degrees() + 180.) / 360.;
  const double y = (1. - std::log(std::tan(M_PI_4 +
                                           latitude.Radians() / 2.)) / M_PI)
    / 2.;
  return {x, y};
}

double
Camera::WorldSizePixels() const noexcept
{
  return TILE_SIZE * std::exp2(zoom);
}

DoublePoint2D
Camera::GeoToScreen(const GeoPoint &p) const noexcept
{
  const double world_size = WorldSizePixels();

  const auto w = GeoToWorld(p);
  const auto w0 = GeoToWorld(center);

  /* the point's offset from the camera center in (unrotated) world
     pixels; the x axis wraps around the antimeridian */
  double dx = (w.x - w0.x) * world_size;
  if (dx > world_size / 2)
    dx -= world_size;
  else if (dx < -world_size / 2)
    dx += world_size;

  const double dy = (w.y - w0.y) * world_size;

  /* rotate by the bearing: with screen coordinates growing right/down,
     a point north of the center (dy < 0) must appear rotated
     counter-clockwise by the bearing angle */
  const double cos_bearing = bearing.cos(), sin_bearing = bearing.sin();
  const double sx = dx * cos_bearing + dy * sin_bearing;
  const double sy = -dx * sin_bearing + dy * cos_bearing;

  return {viewport.width / 2. + sx, viewport.height / 2. + sy};
}

Camera
CameraFromProjection(const WindowProjection &projection) noexcept
{
  Camera camera;

  /* MapLibre's camera center is the geographic point in the middle of
     the viewport; XCSoar's screen origin is elsewhere when the
     aircraft symbol is offset, so ask the projection what is
     currently displayed at the viewport center */
  camera.center = projection.ScreenToGeo(projection.GetScreenCenter());

  camera.bearing = projection.GetScreenAngle().AsBearing();
  camera.viewport = projection.GetScreenSize();

  /* Choose the zoom level so that MapLibre's degrees-to-pixels scale
     at the viewport center matches XCSoar's flat-earth projection.
     Note that this deliberately uses the same sphere radius as
     XCSoar's projection (FAISphere::REARTH, not the WGS84 equatorial
     radius): the basemap must align with the Canvas overlays on
     screen; the resulting ~0.1% deviation from the nominal Web
     Mercator ground resolution has no visible effect. */
  const double world_size = M_2PI * FAISphere::REARTH
    * camera.center.latitude.cos() * projection.GetScale();
  camera.zoom = std::log2(world_size / Camera::TILE_SIZE);

  /* clamp to MapLibre's supported zoom range */
  camera.zoom = std::clamp(camera.zoom, 0., 25.5);

  return camera;
}

} // namespace MapLibre
