// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "LabelShape.hpp"
#include "WaypointRendererSettings.hpp"

#include <climits>
#include <cstddef>

struct Waypoint;

/**
 * The values which can be drawn with a waypoint label, in user units.
 */
struct WaypointArrivalValues {
  /** straight glide arrival height; INT_MIN if unavailable */
  int height_straight = INT_MIN;

  /** terrain avoidance arrival height; INT_MIN if unavailable */
  int height_terrain = INT_MIN;

  /** the altitude at which the waypoint is reached; INT_MIN if
      unavailable */
  int altitude = INT_MIN;

  /** the required glide ratio; 0 if unavailable */
  double glide_ratio = 0;

  /** is the difference between both heights worth the space? */
  bool considerable_delta = false;
};

/**
 * How a waypoint label is drawn.
 */
struct WaypointLabelAppearance {
  LabelShape shape;
  bool bold;
};

/**
 * Look up the appearance of a waypoint label.
 *
 * @param highlighted a reachable landable, a task waypoint or a
 * watched waypoint
 */
[[gnu::pure]]
WaypointLabelAppearance
GetWaypointLabelAppearance(const WaypointRendererSettings &settings,
                           bool highlighted) noexcept;

/**
 * Format the name part of a waypoint label.
 */
void
FormatWaypointLabelTitle(char *buffer, size_t buffer_size,
                         const WaypointRendererSettings &settings,
                         const Waypoint &way_point) noexcept;

/**
 * Format the arrival info which is drawn with a waypoint label.
 *
 * @return false if there is nothing to draw
 */
bool
FormatWaypointArrivalInfo(char *buffer, size_t buffer_size,
                          const WaypointRendererSettings &settings,
                          const WaypointArrivalValues &values,
                          const char *altitude_unit) noexcept;
