// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindowProjection.hpp"
#include "Geo/Quadrilateral.hpp"

std::optional<PixelPoint>
WindowProjection::GeoToScreenIfVisible(const GeoPoint &loc) const noexcept
{
  if (!GeoVisible(loc))
    return {};

  auto p = GeoToScreen(loc);
  if (!ScreenVisible(p))
    return {};

  return p;
}

void
WindowProjection::SetScaleFromRadius(double radius) noexcept
{
  SetScale(double(GetMinScreenDistance()) / (radius * 2));
}

double
WindowProjection::GetMapScale() const noexcept
{
  return DistancePixelsToMeters(GetMapResolutionFactor());
}

double
WindowProjection::GetScreenDistanceMeters() const noexcept
{
  return DistancePixelsToMeters(GetScreenDistance());
}

GeoPoint
WindowProjection::GetGeoScreenCenter() const noexcept
{
  return ScreenToGeo(GetScreenCenter());
}

GeoQuadrilateral
WindowProjection::GetGeoQuadrilateral() const noexcept
{
  const auto r = GetScreenRect();
  return {
    ScreenToGeo(r.GetTopLeft()),
    ScreenToGeo(r.GetTopRight()),
    ScreenToGeo(r.GetBottomLeft()),
    ScreenToGeo(r.GetBottomRight()),
  };
}

void
WindowProjection::UpdateScreenBounds() noexcept
{
  assert(screen_size_initialised);

  if (!IsValid())
    return;

  screen_bounds = GetGeoQuadrilateral().GetBounds();
}

void
WindowProjection::UpdateScreenTilt(Angle tilt) noexcept
{
  if (tilt <= Angle::Zero()) {
    SetScreenTilt(Angle::Zero(), 0);
    return;
  }

  /* the horizon of the tilted plane lies distance/tan(tilt) above
     the ScreenOrigin; scaling the camera distance with tan(tilt)
     guarantees it stays at least 1.5 screen heights away, i.e. safely
     off-screen for all tilt angles */
  const double height = GetScreenSize().height;
  const double distance = 1.5 * height * std::max(1., tilt.tan());

  SetScreenTilt(tilt, distance);
}
