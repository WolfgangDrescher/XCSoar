// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"
#include "Math/Angle.hpp"
#include "Math/Point2D.hpp"
#include "ui/dim/Size.hpp"

namespace MapLibre {

/**
 * A camera description for a MapLibre map view, equivalent to
 * mbgl::CameraOptions with pitch fixed to zero.
 *
 * This struct deliberately has no dependency on MapLibre headers so
 * it can be unit-tested and compiled in builds without
 * ENABLE_MAPLIBRE.
 */
struct Camera {
  /**
   * MapLibre's logical tile size [px]: at zoom level z, the whole Web
   * Mercator world is TILE_SIZE * 2^z pixels wide.
   */
  static constexpr double TILE_SIZE = 512;

  /** the geographic point at the center of the viewport */
  GeoPoint center;

  /** continuous MapLibre zoom level (based on #TILE_SIZE) */
  double zoom;

  /**
   * The compass direction that is "up" on the screen; same
   * convention as Projection::GetScreenAngle() and MapLibre's
   * "bearing", normalised to [0, 360).
   */
  Angle bearing;

  /** viewport size [px] */
  PixelSize viewport;

  /** the width of the whole Web Mercator world at #zoom [px] */
  [[gnu::pure]]
  double WorldSizePixels() const noexcept;

  /**
   * Project a #GeoPoint to viewport coordinates (x right, y down,
   * origin at the top left corner of the viewport) the way a MapLibre
   * map configured with this camera does: Web Mercator, rotated by
   * #bearing, #center in the middle of the viewport.
   *
   * This is a reference implementation used to verify the alignment
   * with XCSoar's flat-earth #Projection in unit tests and for
   * diagnostics; the actual basemap rendering happens inside MapLibre
   * itself.
   */
  [[gnu::pure]]
  DoublePoint2D GeoToScreen(const GeoPoint &p) const noexcept;
};

} // namespace MapLibre
