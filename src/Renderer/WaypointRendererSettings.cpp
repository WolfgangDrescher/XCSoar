// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WaypointRendererSettings.hpp"
#include "LabelShape.hpp"
#include "Profile/Profile.hpp"

/**
 * Migrate the obsolete "WaypointArrivalHeightDisplay" setting, which
 * combined the arrival info with the way it was calculated.
 */
void
WaypointRendererSettings::MigrateArrivalHeightDisplay() noexcept
{
  enum class Obsolete : uint8_t {
    NONE = 0,
    GLIDE,
    TERRAIN,
    GLIDE_AND_TERRAIN,
    REQUIRED_GR,
    REQUIRED_GR_AND_TERRAIN,
  } value = Obsolete::GLIDE;

  if (!Profile::GetEnum(ProfileKeys::WaypointArrivalHeightDisplay, value))
    return;

  switch (value) {
  case Obsolete::NONE:
    arrival_info = ArrivalInfo::NONE;
    break;

  case Obsolete::GLIDE:
    arrival_info = ArrivalInfo::ARRIVAL_HEIGHT;
    arrival_calculation = ArrivalCalculation::STRAIGHT;
    break;

  case Obsolete::TERRAIN:
    arrival_info = ArrivalInfo::ARRIVAL_HEIGHT;
    arrival_calculation = ArrivalCalculation::TERRAIN;
    break;

  case Obsolete::GLIDE_AND_TERRAIN:
    arrival_info = ArrivalInfo::ARRIVAL_HEIGHT;
    arrival_calculation = ArrivalCalculation::BOTH;
    break;

  case Obsolete::REQUIRED_GR:
    arrival_info = ArrivalInfo::GLIDE_RATIO;
    break;

  case Obsolete::REQUIRED_GR_AND_TERRAIN:
    arrival_info = ArrivalInfo::BOTH;
    arrival_calculation = ArrivalCalculation::TERRAIN;
    break;
  }
}

/**
 * Migrate the obsolete "WaypointLabelStyle" setting, which applied to
 * the reachable landables only.
 */
void
WaypointRendererSettings::MigrateLabelStyle() noexcept
{
  LabelShape shape = LabelShape::ROUNDED_BLACK;
  if (!Profile::GetEnum(ProfileKeys::WaypointLabelStyle, shape))
    return;

  highlight_style = shape == LabelShape::OUTLINED_INVERTED
    ? HighlightStyle::OUTLINED_INVERTED
    : HighlightStyle::BADGE;
}

void
WaypointRendererSettings::LoadFromProfile() noexcept
{
  using namespace Profile;

  // NOTE: WaypointLabelSelection must be loaded after this code
  GetEnum(ProfileKeys::DisplayText, display_text_type);
  if (display_text_type == DisplayTextType::OBSOLETE_DONT_USE_NAMEIFINTASK) {
    // pref migration. The migrated value of DisplayTextType and
    // WaypointLabelSelection will not be written to the config file
    // unless the user explicitly changes the corresponding setting manually.
    // This requires ordering because a manually changed WaypointLabelSelection
    // may be overwritten by the following migration code.
    display_text_type = DisplayTextType::NAME;
    label_selection = LabelSelection::TASK;
  } else if (display_text_type == DisplayTextType::OBSOLETE_DONT_USE_NUMBER)
    display_text_type = DisplayTextType::NAME;

  // NOTE: DisplayTextType must be loaded before this code
  //       due to pref migration dependencies!
  GetEnum(ProfileKeys::WaypointLabelSelection, label_selection);

  GetEnum(ProfileKeys::WaypointArrivalCalculation, arrival_calculation);
  GetEnum(ProfileKeys::WaypointArrivalInfoPosition, arrival_info_position);
  GetEnum(ProfileKeys::WaypointArrivalInfoVisibility, arrival_info_visibility);

  // pref migration; only when this profile has never seen the new settings
  if (!GetEnum(ProfileKeys::WaypointArrivalInfo, arrival_info))
    MigrateArrivalHeightDisplay();

  GetEnum(ProfileKeys::WaypointTextStyle, label_style);
  if (!GetEnum(ProfileKeys::WaypointHighlightStyle, highlight_style))
    MigrateLabelStyle();

  GetEnum(ProfileKeys::AppIndLandable, landable_style);
  Get(ProfileKeys::AppUseSWLandablesRendering, vector_landable_rendering);
  Get(ProfileKeys::AppScaleRunwayLength, scale_runway_length);
  Get(ProfileKeys::AppLandableRenderingScale, landable_rendering_scale);
  Get(ProfileKeys::MapWaypointIconScale, map_waypoint_icon_scale);
}
