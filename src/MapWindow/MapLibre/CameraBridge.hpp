// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Camera.hpp"

class WindowProjection;

namespace MapLibre {

/**
 * Derive the MapLibre camera that shows the same map extent as the
 * given XCSoar projection.
 *
 * The zoom level is chosen so that MapLibre's degrees-to-pixels scale
 * at the viewport center matches XCSoar's flat-earth projection;
 * towards the viewport edges the two projections diverge slightly
 * (flat-earth vs. Web Mercator), which is a property of the map
 * content, not of this conversion.  See TestCameraBridge for the
 * measured bounds of that divergence.
 *
 * The projection must be valid (Projection::IsValid()) and must have
 * a screen size.
 */
[[gnu::pure]]
Camera
CameraFromProjection(const WindowProjection &projection) noexcept;

} // namespace MapLibre
