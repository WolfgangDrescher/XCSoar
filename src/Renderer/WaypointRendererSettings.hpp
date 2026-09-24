// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

struct WaypointRendererSettings {
  /** What type of text to draw next to the waypoint icon */
  enum class DisplayTextType : uint8_t {
    NAME = 0,
    OBSOLETE_DONT_USE_NUMBER,
    FIRST_FIVE,
    NONE,
    FIRST_THREE,
    OBSOLETE_DONT_USE_NAMEIFINTASK,
    FIRST_WORD,
    SHORT_NAME,
  } display_text_type;

  /** Which arrival height to display next to waypoint labels */

  enum class ArrivalHeightDisplay : uint8_t {
    NONE = 0,
    GLIDE,
    TERRAIN,
    GLIDE_AND_TERRAIN,
    REQUIRED_GR,
    REQUIRED_GR_AND_TERRAIN,
  } arrival_height_display;

  /** What type of waypoint labels to render */
  enum class LabelSelection : uint8_t {
    ALL,
    TASK_AND_LANDABLE,
    TASK,
    NONE,
    TASK_AND_AIRFIELD,
  } label_selection;

  /**
   * The style of the waypoint labels which get a box; for now, these
   * are the labels of reachable landables.  The values are stored in
   * the profile; they are those of the former LabelShape.
   */
  enum class WaypointLabelStyle : uint8_t {
    OUTLINED = 3,
    ROUNDED_RECTANGLE = 5,
  } label_style;

  enum class LandableStyle : uint8_t {
    PURPLE_CIRCLE,
    BW,
    TRAFFIC_LIGHTS,
  } landable_style;

  bool vector_landable_rendering;

  bool scale_runway_length;

  int landable_rendering_scale;

  /**
   * Map waypoint symbol size in percent of intrinsic icon / vector scale
   * (50–200; Configuration → Map display → Waypoints).
   */
  int map_waypoint_icon_scale;

  void SetDefaults() noexcept {
    display_text_type = DisplayTextType::SHORT_NAME;
    arrival_height_display = ArrivalHeightDisplay::GLIDE;
    label_selection = LabelSelection::ALL;
    label_style = WaypointLabelStyle::ROUNDED_RECTANGLE;

    landable_style = LandableStyle::PURPLE_CIRCLE;
    vector_landable_rendering = true;
    scale_runway_length = false;
    landable_rendering_scale = 100;
    map_waypoint_icon_scale = 100;
  }

  void LoadFromProfile() noexcept;
};
