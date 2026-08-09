// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Math/Angle.hpp"

class Canvas;
class Projection;
struct GeoPoint;

/**
 * Draws a circle around a geographic center with a radius in meters.
 *
 * A circle on the ground is only a circle on the screen while the map
 * is viewed straight from above.  With a tilted #Projection it is an
 * ellipse, so the outline is projected point by point; without tilt
 * this falls back to Canvas::DrawCircle() and costs nothing extra.
 */
void
DrawGeoCircle(Canvas &canvas, const Projection &projection,
              const GeoPoint &center, double radius) noexcept;

/**
 * Like DrawGeoCircle(), but draws only the sector between the two
 * radials (including the center point), analogous to
 * Canvas::DrawSegment().
 *
 * @param start the start radial as a true geographic bearing
 * @param end the end radial as a true geographic bearing
 */
void
DrawGeoSegment(Canvas &canvas, const Projection &projection,
               const GeoPoint &center, double radius,
               Angle start, Angle end) noexcept;

/**
 * Like DrawGeoSegment(), but leaves out the area within
 * @p inner_radius, analogous to Canvas::DrawAnnulus().
 */
void
DrawGeoAnnulus(Canvas &canvas, const Projection &projection,
               const GeoPoint &center, double inner_radius, double radius,
               Angle start, Angle end) noexcept;

/**
 * Draws the union of the sector between the two radials and the full
 * circle of @p inner_radius, analogous to Canvas::DrawKeyhole().
 */
void
DrawGeoKeyhole(Canvas &canvas, const Projection &projection,
               const GeoPoint &center, double inner_radius, double radius,
               Angle start, Angle end) noexcept;
