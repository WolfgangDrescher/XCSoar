// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Math/Angle.hpp"

class Projection;
struct GeoPoint;
struct BulkPixelPoint;

/**
 * The maximum number of segments used to approximate a full circle.
 */
static constexpr unsigned MAX_GEO_CIRCLE_SEGMENTS = 128;

/**
 * The maximum number of points ProjectGeoArc() may write.
 */
static constexpr unsigned MAX_GEO_ARC_POINTS = MAX_GEO_CIRCLE_SEGMENTS + 1;

/**
 * Projects the outline of a circle around a geographic center to
 * screen coordinates.
 *
 * A circle on the ground is only a circle on the screen while the map
 * is viewed straight from above; with a tilted #Projection it is an
 * ellipse.  Projecting the outline point by point is correct for
 * either case.
 *
 * The outline is not closed explicitly because the polygon drawing
 * functions close it anyway.
 *
 * @param radius the radius in meters
 * @param points receives the outline; room for
 * #MAX_GEO_CIRCLE_SEGMENTS items is required
 * @return the number of points written
 */
unsigned
ProjectGeoCircle(const Projection &projection, const GeoPoint &center,
                 double radius, BulkPixelPoint *points) noexcept;

/**
 * Projects an arc around a geographic center to screen coordinates,
 * running clockwise from @p start to @p end.  Both radials are true
 * geographic bearings; the screen rotation is applied by the
 * #Projection.
 *
 * A degenerate arc is interpreted as a full circle, following the
 * convention of Canvas::DrawSegment().
 *
 * @param points receives the arc; room for #MAX_GEO_ARC_POINTS items
 * is required
 * @return the number of points written
 */
unsigned
ProjectGeoArc(const Projection &projection, const GeoPoint &center,
              double radius, Angle start, Angle end,
              BulkPixelPoint *points) noexcept;

/**
 * The angle spanned by an arc running clockwise from @p start to
 * @p end, in degrees; a degenerate arc yields a full circle.
 */
[[gnu::const]]
double
CalcGeoArcSweep(Angle start, Angle end) noexcept;

/**
 * The number of segments ProjectGeoArc() uses for an arc of @p sweep
 * degrees.
 */
[[gnu::pure]]
unsigned
CalcGeoArcSegments(const Projection &projection, double radius,
                   double sweep) noexcept;

/**
 * Projects @p count points spaced @p step degrees apart, starting at
 * @p start.  A negative @p step runs counter-clockwise.
 */
void
ProjectGeoArcPoints(const Projection &projection, const GeoPoint &center,
                    double radius, Angle start, double step, unsigned count,
                    BulkPixelPoint *points) noexcept;
