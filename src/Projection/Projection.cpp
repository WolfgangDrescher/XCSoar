// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Projection.hpp"
#include "Geo/FAISphere.hpp"
#include "Math/Angle.hpp"

#include <algorithm>

Projection::Projection() noexcept
{
  SetScale(1);
}

void
Projection::SetScreenTilt(Angle tilt, double perspective_distance) noexcept
{
  if (tilt <= Angle::Zero() || perspective_distance <= 0) {
    screen_tilt = Angle::Zero();
    tilt_cos = 1;
    tilt_foreshortening = 0;
    return;
  }

  screen_tilt = tilt;

  const auto [tilt_sin, _tilt_cos] = tilt.SinCos();
  tilt_cos = _tilt_cos;
  tilt_foreshortening = tilt_sin / perspective_distance;
}

/**
 * Points on the screen may lie beyond the horizon of the tilted map
 * plane (or far behind the virtual camera); clamping the perspective
 * divisor keeps the conversions finite there.
 */
static constexpr double MIN_TILT_DIVISOR = 0.05;

DoublePoint2D
Projection::ApplyScreenTilt(DoublePoint2D v) const noexcept
{
  if (!HasScreenTilt())
    return v;

  /* negative y is the far half of the map; the divisor grows with
     distance, shrinking far objects towards the horizon */
  const double w = std::max(1 - v.y * tilt_foreshortening,
                            MIN_TILT_DIVISOR);
  return {v.x / w, v.y * tilt_cos / w};
}

DoublePoint2D
Projection::InverseScreenTilt(DoublePoint2D v) const noexcept
{
  if (!HasScreenTilt())
    return v;

  const double divisor = std::max(tilt_cos + v.y * tilt_foreshortening,
                                  MIN_TILT_DIVISOR);
  const double y = v.y / divisor;
  const double w = tilt_cos / divisor;
  return {v.x * w, y};
}

GeoPoint
Projection::ScreenToGeo(PixelPoint src) const noexcept
{
  assert(IsValid());

  PixelPoint rel = src - screen_origin;
  if (HasScreenTilt()) {
    const auto untilted = InverseScreenTilt(DoublePoint2D(rel.x, rel.y));
    rel = {iround(untilted.x), iround(untilted.y)};
  }

  const auto p =
    screen_rotation.Rotate(rel);

  GeoPoint g(PixelsToAngle(p.x), PixelsToAngle(p.y));

  g.latitude = geo_location.latitude - g.latitude;

  /* paranoid sanity check to avoid integer overflow near the poles;
     our projection isn't doing well at all there; this check avoids
     assertion failures when the user pans all the way up/down */
  const Angle latitude(std::min(Angle::Degrees(80),
                                std::max(Angle::Degrees(-80), g.latitude)));

  g.longitude = geo_location.longitude + g.longitude * latitude.invfastcosine();

  return g;
}

PixelPoint
Projection::GeoToScreen(const GeoPoint &g) const noexcept
{
  assert(IsValid());

  const GeoPoint d = geo_location-g;

  const auto p =
    screen_rotation.Rotate(PixelPoint(int(g.latitude.fastcosine() *
                                          AngleToPixels(d.longitude)),
                                      (int)AngleToPixels(d.latitude)));

  if (HasScreenTilt()) {
    const auto tilted = ApplyScreenTilt(DoublePoint2D(-p.x, p.y));
    return screen_origin + PixelPoint{iround(tilted.x), iround(tilted.y)};
  }

  PixelPoint sc;
  sc.x = screen_origin.x - p.x;
  sc.y = screen_origin.y + p.y;
  return sc;
}

void
Projection::SetScale(const double _scale) noexcept
{
  scale = _scale;

  // Calculate earth radius in pixels
  draw_scale = FAISphere::REARTH * scale;
  // Save inverted value for faster calculations
  inv_draw_scale = 1. / draw_scale;
}
