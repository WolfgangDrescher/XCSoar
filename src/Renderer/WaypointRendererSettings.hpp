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

  /**
   * Which arrival info to display with waypoint labels.  This is a
   * bit set: the height above the safety arrival height, the glide
   * ratio which is required to get there, and the altitude at which
   * one arrives.
   */
  enum class ArrivalInfo : uint8_t {
    NONE = 0,
    HEIGHT = 0x1,
    GLIDE_RATIO = 0x2,
    HEIGHT_AND_GLIDE_RATIO = HEIGHT | GLIDE_RATIO,
    ALTITUDE = 0x4,
    ALTITUDE_AND_GLIDE_RATIO = ALTITUDE | GLIDE_RATIO,
    HEIGHT_AND_ALTITUDE = HEIGHT | ALTITUDE,
    ALL = HEIGHT | ALTITUDE | GLIDE_RATIO,
  } arrival_info;

  static constexpr bool Contains(ArrivalInfo value,
                                 ArrivalInfo bit) noexcept {
    return ((uint8_t)value & (uint8_t)bit) != 0;
  }

  /** How the arrival height is calculated */
  enum class ArrivalCalculation : uint8_t {
    STRAIGHT = 0,
    TERRAIN,
    BOTH,
  } arrival_calculation;

  /** Where the arrival info is drawn */
  enum class ArrivalInfoPosition : uint8_t {
    AFTER_NAME = 0,
    BADGE_BELOW,
  } arrival_info_position;

  /** Which waypoints get arrival info */
  enum class ArrivalInfoVisibility : uint8_t {
    REACHABLE = 0,
    ALL,
    LANDABLE,
  } arrival_info_visibility;

  /** How waypoint labels are drawn */
  enum class LabelStyle : uint8_t {
    TEXT = 0,
    OUTLINED,
    OUTLINED_INVERTED,
    BADGE,
  } label_style;

  /**
   * How the labels of highlighted waypoints are drawn; those are the
   * reachable landables, the task waypoints and the watched
   * waypoints.  Everything but NONE also makes them bold.
   */
  enum class HighlightStyle : uint8_t {
    NONE = 0,
    BOLD,
    OUTLINED,
    OUTLINED_INVERTED,
    BADGE,
  } highlight_style;

  /** What type of waypoint labels to render */
  enum class LabelSelection : uint8_t {
    ALL,
    TASK_AND_LANDABLE,
    TASK,
    NONE,
    TASK_AND_AIRFIELD,
  } label_selection;

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
    arrival_info = ArrivalInfo::HEIGHT;
    arrival_calculation = ArrivalCalculation::STRAIGHT;
    arrival_info_position = ArrivalInfoPosition::AFTER_NAME;
    arrival_info_visibility = ArrivalInfoVisibility::REACHABLE;
    label_selection = LabelSelection::ALL;
    label_style = LabelStyle::OUTLINED;
    highlight_style = HighlightStyle::BADGE;

    landable_style = LandableStyle::PURPLE_CIRCLE;
    vector_landable_rendering = true;
    scale_runway_length = false;
    landable_rendering_scale = 100;
    map_waypoint_icon_scale = 100;
  }

  void LoadFromProfile() noexcept;

private:
  void MigrateArrivalHeightDisplay() noexcept;
  void MigrateLabelStyle() noexcept;
};
