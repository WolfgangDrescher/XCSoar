// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TurnBackMarkerRenderer.hpp"
#include "Look/MapLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Math/Angle.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"
#include "Projection/WindowProjection.hpp"
#include "Geo/Math.hpp"
#include "Screen/Layout.hpp"
#include "Math/Screen.hpp"
#include "Math/Util.hpp"
#include "Computer/Settings.hpp"
#include <cmath>

// Small value to avoid division by zero and floating point issues
static constexpr double TBM_EPSILON = 1e-6;

void
TurnBackMarkerRenderer::Draw(Canvas &canvas,
                             const WindowProjection &projection,
                             const NMEAInfo &basic,
                             const DerivedInfo &calculated,
                             const ComputerSettings &settings) const noexcept
{
  // Check if the feature is enabled in settings
  if (!settings.task.turn_back_marker_enabled)
    return;

  // Check if we have a valid task and solution data structure
  const TaskStats &task_stats = calculated.task_stats;
  if (!task_stats.task_valid)
    return;

  const GlideResult &solution = task_stats.current_leg.solution_remaining;

  // Need a valid solution to proceed (even if below glide for the *old* TBM)
  if (!solution.IsOk())
    return;

  const double alt_diff = solution.SelectAltitudeDifference(settings.task.glide);
  if (alt_diff <= 0) {
    return;
  }

  // Check if we have a valid track
  if (!basic.track_available)
    return;

  // Get the current task waypoint location
  const GeoPoint &waypoint_location = task_stats.current_leg.location_remaining;
  if (!waypoint_location.IsValid())
    return;

  // Calculate direct distance (C) and bearing to waypoint from current position
  const GeoVector direct_vector = basic.location.DistanceBearing(waypoint_location);
  if (!direct_vector.IsValid() || direct_vector.distance < TBM_EPSILON) // Avoid issues if already at WP
    return;
  const double C_direct_distance = direct_vector.distance;

  // Calculate angle (theta) between current track and direct path to waypoint
  const Angle track_to_waypoint_angle = (basic.track - direct_vector.bearing).AsDelta();
  const double cos_theta = track_to_waypoint_angle.cos();

  // --- NEW TBM CALCULATION ---

  // Estimate total altitude available above the target arrival point at the waypoint
  // This assumes solution targets waypoint altitude correctly.
  const double Alt_Total = solution.height_glide + alt_diff;

  // We need positive altitude relative to the waypoint to glide there
  if (Alt_Total <= TBM_EPSILON) {
    return; // Cannot reach waypoint even directly, let alone via TBM
  }

  // Calculate the effective glide ratio *required* for the direct leg
  // Use this as an estimate for the detour legs. Need valid height_glide.
  if (solution.height_glide <= TBM_EPSILON) {
    // Cannot reliably calculate required glide ratio
    return;
  }
  const double glide_ratio = C_direct_distance / solution.height_glide;
  if (glide_ratio <= TBM_EPSILON) {
    // Invalid glide ratio
    return;
  }

  // Calculate D_Total: Total horizontal distance achievable with Alt_Total
  const double D_Total = Alt_Total * glide_ratio;

  // Calculate the denominator for the solution of x
  const double denominator = 2.0 * (D_Total - C_direct_distance * cos_theta);

  // Check for degenerate case (denominator close to zero)
  if (std::abs(denominator) < TBM_EPSILON) {
    // Geometric configuration prevents a stable solution here
    return;
  }

  // Calculate distance_to_tbm (x)
  // x = (D_Total^2 - C^2) / (2 * (D_Total - C * cos(theta)))
  const double distance_to_tbm = (D_Total * D_Total - C_direct_distance * C_direct_distance) / denominator;

  // --- Validity Checks for the solution x ---

  // 1. TBM must be ahead of the aircraft.
  if (distance_to_tbm < 0.0) {
    // The calculated point is behind the current position.
    return;
  }

  // 2. The geometry requires D_Total - x >= 0 (from sqrt step)
  //    This means x <= D_Total. If x > D_Total, the geometry is impossible.
  if (distance_to_tbm > D_Total + TBM_EPSILON) { // Add epsilon for float tolerance
    // This implies the required distance from TBM to WP would be negative.
    return;
  }

  // --- TBM Calculation Successful ---

  // Find the GEOGRAPHIC LOCATION of the TBM
  GeoPoint tbm_location = FindLatitudeLongitude(basic.location,
                                                basic.track,
                                                distance_to_tbm);

  // Convert the geographic TBM location to screen coordinates
  auto tbm_screen = projection.GeoToScreenIfVisible(tbm_location);
  if (!tbm_screen) {
    // TBM is calculated but currently off-screen
    return;
  }

  // --- Draw the TBM Symbol ---
  // Track-local coords: -Y is ahead on track, +Y is toward the aircraft.
  // Green bar at the limit, filled chevron aft, red bar ahead.
  const int bar_half = Layout::Scale(4);
  const int chev_spread = Layout::Scale(3);
  // Equilateral triangle: depth = half-base * sqrt(3)
  const int chev_depth = iround(double(chev_spread) * 1.7320508075688772);
  const int far_gap = Layout::Scale(1);

  BulkPixelPoint bar[2] = {
    { -bar_half, 0 },
    { bar_half, 0 },
  };
  BulkPixelPoint far_bar[2] = {
    { -bar_half, -far_gap },
    { bar_half, -far_gap },
  };
  BulkPixelPoint chev[3] = {
    { 0, 0 },
    { -chev_spread, chev_depth },
    { chev_spread, chev_depth },
  };

  const Angle angle = basic.track - projection.GetScreenAngle();
  PolygonRotateShift(std::span<BulkPixelPoint>(bar, 2), *tbm_screen, angle);
  PolygonRotateShift(std::span<BulkPixelPoint>(far_bar, 2), *tbm_screen, angle);
  PolygonRotateShift(std::span<BulkPixelPoint>(chev, 3), *tbm_screen, angle);

  canvas.Select(look.tbm_pen);
  canvas.DrawLine(bar[0], bar[1]);

  canvas.Select(look.tbm_brush);
  canvas.DrawPolygon(chev, 3);

  canvas.Select(look.tbm_far_pen);
  canvas.DrawLine(far_bar[0], far_bar[1]);
}
