// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WaypointLabelFormat.hpp"
#include "WaypointRendererSettings.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "util/Compiler.h"
#include "util/StringFormat.hpp"
#include "util/TruncateString.hpp"

#include <cassert>
#include <math.h>
#include <string.h>

[[gnu::const]]
static LabelShape
ToLabelShape(WaypointRendererSettings::LabelStyle style) noexcept
{
  switch (style) {
  case WaypointRendererSettings::LabelStyle::OUTLINED:
    return LabelShape::OUTLINED;

  case WaypointRendererSettings::LabelStyle::OUTLINED_INVERTED:
    return LabelShape::OUTLINED_INVERTED;

  case WaypointRendererSettings::LabelStyle::BADGE:
    return LabelShape::ROUNDED_WHITE;

  case WaypointRendererSettings::LabelStyle::TEXT:
    break;
  }

  return LabelShape::SIMPLE;
}

WaypointLabelAppearance
GetWaypointLabelAppearance(const WaypointRendererSettings &settings,
                           bool highlighted) noexcept
{
  WaypointLabelAppearance appearance{ToLabelShape(settings.label_style),
                                     false};

  if (!highlighted)
    return appearance;

  switch (settings.highlight_style) {
  case WaypointRendererSettings::HighlightStyle::NONE:
    return appearance;

  case WaypointRendererSettings::HighlightStyle::BOLD:
    break;

  case WaypointRendererSettings::HighlightStyle::OUTLINED:
    appearance.shape = LabelShape::OUTLINED;
    break;

  case WaypointRendererSettings::HighlightStyle::OUTLINED_INVERTED:
    appearance.shape = LabelShape::OUTLINED_INVERTED;
    break;

  case WaypointRendererSettings::HighlightStyle::BADGE:
    appearance.shape = LabelShape::ROUNDED_WHITE;
    break;
  }

  appearance.bold = true;
  return appearance;
}

void
FormatWaypointLabelTitle(char *buffer, size_t buffer_size,
                         const WaypointRendererSettings &settings,
                         const Waypoint &way_point) noexcept
{
  buffer[0] = '\0';

  switch (settings.display_text_type) {
  case WaypointRendererSettings::DisplayTextType::NAME:
    CopyTruncateString(buffer, buffer_size, way_point.name.c_str());
    break;

  case WaypointRendererSettings::DisplayTextType::FIRST_FIVE:
    CopyTruncateString(buffer, buffer_size, way_point.name.c_str(), 5);
    break;

  case WaypointRendererSettings::DisplayTextType::FIRST_THREE:
    CopyTruncateString(buffer, buffer_size, way_point.name.c_str(), 3);
    break;

  case WaypointRendererSettings::DisplayTextType::NONE:
    break;

  case WaypointRendererSettings::DisplayTextType::FIRST_WORD:
    CopyTruncateString(buffer, buffer_size, way_point.name.c_str());
    if (char *space = strstr(buffer, " "); space != nullptr)
      space[0] = '\0';
    break;

  case WaypointRendererSettings::DisplayTextType::SHORT_NAME:
    if (!way_point.shortname.empty())
      CopyTruncateString(buffer, buffer_size, way_point.shortname.c_str());
    else
      CopyTruncateString(buffer, buffer_size, way_point.name.c_str(), 5);
    break;

  case WaypointRendererSettings::DisplayTextType::OBSOLETE_DONT_USE_NUMBER:
  case WaypointRendererSettings::DisplayTextType::OBSOLETE_DONT_USE_NAMEIFINTASK:
    assert(false);
    gcc_unreachable();
  }
}

/**
 * @return the number of characters written
 */
static size_t
FormatArrivalHeight(char *buffer, size_t buffer_size,
                    const WaypointRendererSettings &settings,
                    const WaypointArrivalValues &values,
                    const char *altitude_unit) noexcept
{
  switch (settings.arrival_calculation) {
  case WaypointRendererSettings::ArrivalCalculation::TERRAIN:
    if (values.height_terrain == INT_MIN)
      return 0;

    StringFormat(buffer, buffer_size, "%d%s", values.height_terrain,
                 altitude_unit);
    return strlen(buffer);

  case WaypointRendererSettings::ArrivalCalculation::BOTH:
    /* two values are only worth the space if the detour really costs
       something */
    if (values.height_straight != INT_MIN &&
        values.height_terrain != INT_MIN && values.considerable_delta) {
      StringFormat(buffer, buffer_size, "%d/%d%s", values.height_straight,
                   values.height_terrain, altitude_unit);
      return strlen(buffer);
    }

    break;

  case WaypointRendererSettings::ArrivalCalculation::STRAIGHT:
    break;
  }

  if (values.height_straight == INT_MIN)
    return 0;

  StringFormat(buffer, buffer_size, "%d%s", values.height_straight,
               altitude_unit);
  return strlen(buffer);
}

bool
FormatWaypointArrivalInfo(char *buffer, size_t buffer_size,
                          const WaypointRendererSettings &settings,
                          const WaypointArrivalValues &values,
                          const char *altitude_unit) noexcept
{
  assert(buffer_size > 8);

  buffer[0] = '\0';

  if (settings.arrival_info == WaypointRendererSettings::ArrivalInfo::NONE)
    return false;

  size_t length = 0;

  if (settings.arrival_info !=
      WaypointRendererSettings::ArrivalInfo::GLIDE_RATIO)
    length = FormatArrivalHeight(buffer, buffer_size, settings, values,
                                 altitude_unit);

  if (settings.arrival_info !=
      WaypointRendererSettings::ArrivalInfo::ARRIVAL_HEIGHT &&
      values.glide_ratio > 0) {
    /* a line of its own, separated by a rule inside the label */
    if (length > 0 && length + 1 < buffer_size)
      buffer[length++] = '\n';

    StringFormat(buffer + length, buffer_size - length, "%d",
                 (int)lround(values.glide_ratio));
  }

  return buffer[0] != '\0';
}
