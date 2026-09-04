// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WaypointDisplayConfigPanel.hpp"
#include "Profile/Keys.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Listener.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"

enum ControlIndex {
  WaypointLabels,
  WaypointLabelSelection,
  WaypointTextStyle,
  WaypointHighlightStyle,
  WaypointArrivalInfo,
  WaypointArrivalCalculation,
  WaypointArrivalInfoPosition,
  WaypointArrivalInfoVisibility,
  AppIndLandable,
  MapWaypointIconScale,
  AppUseSWLandablesRendering,
  AppLandableRenderingScale,
  AppScaleRunwayLength
};

class WaypointDisplayConfigPanel final
  : public RowFormWidget, DataFieldListener {
public:
  WaypointDisplayConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  void UpdateVisibilities();

  /* methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
};

void
WaypointDisplayConfigPanel::UpdateVisibilities()
{
  bool visible = GetValueBoolean(AppUseSWLandablesRendering);
  SetRowVisible(AppLandableRenderingScale, visible);
  SetRowVisible(AppScaleRunwayLength, visible);

  const auto arrival_info = (WaypointRendererSettings::ArrivalInfo)
    GetValueEnum(WaypointArrivalInfo);
  const bool has_arrival_info =
    arrival_info != WaypointRendererSettings::ArrivalInfo::NONE;

  /* the glide ratio is always calculated straight over ground */
  SetRowEnabled(WaypointArrivalCalculation, has_arrival_info &&
                arrival_info != WaypointRendererSettings::ArrivalInfo::GLIDE_RATIO);
  SetRowEnabled(WaypointArrivalInfoPosition, has_arrival_info);
  SetRowEnabled(WaypointArrivalInfoVisibility, has_arrival_info);
}

void
WaypointDisplayConfigPanel::OnModified(DataField &df) noexcept
{
  if (IsDataField(AppUseSWLandablesRendering, df) ||
      IsDataField(WaypointArrivalInfo, df))
    UpdateVisibilities();
}

void
WaypointDisplayConfigPanel::Prepare(ContainerWindow &parent,
                                    const PixelRect &rc) noexcept
{
  const WaypointRendererSettings &settings = CommonInterface::GetMapSettings().waypoint;

  RowFormWidget::Prepare(parent, rc);

  static constexpr StaticEnumChoice wp_labels_list[] = {
    { WaypointRendererSettings::DisplayTextType::NAME,
      N_("Full name"),
      N_("The full name of each waypoint is displayed.") },
    { WaypointRendererSettings::DisplayTextType::FIRST_WORD,
      N_("First word of name"),
      N_("The first word of the waypoint name is displayed.") },
    { WaypointRendererSettings::DisplayTextType::FIRST_THREE,
      N_("First 3 letters"),
      N_("The first 3 letters of the waypoint name are displayed.") },
    { WaypointRendererSettings::DisplayTextType::FIRST_FIVE,
      N_("First 5 letters"),
      N_("The first 5 letters of the waypoint name are displayed.") },
    { WaypointRendererSettings::DisplayTextType::NONE,
      N_("None"), N_("No waypoint name is displayed.") },
    { WaypointRendererSettings::DisplayTextType::SHORT_NAME,
      N_("Short Name"),
      N_("The short name of each waypoint is displayed. If unavailable, the first five letters of the full name are displayed.") },
    nullptr
  };
  AddEnum(_("Label format"), _("Determines how labels are displayed with each waypoint"),
          wp_labels_list, (unsigned)settings.display_text_type);

  static constexpr StaticEnumChoice wp_selection_list[] = {
    { WaypointRendererSettings::LabelSelection::ALL,
      N_("All"), N_("All labels will be displayed.") },
    { WaypointRendererSettings::LabelSelection::TASK_AND_AIRFIELD,
      N_("Task waypoints & airfields"),
      N_("All waypoints part of a task and all airfields will be displayed.") },
    { WaypointRendererSettings::LabelSelection::TASK_AND_LANDABLE,
      N_("Task waypoints & landables"),
      N_("All waypoints part of a task and all landables will be displayed.") },
    { WaypointRendererSettings::LabelSelection::TASK,
      N_("Task waypoints"),
      N_("All waypoints part of a task will be displayed.") },
    { WaypointRendererSettings::LabelSelection::NONE,
      N_("None"), N_("No labels will be displayed.") },
    nullptr
  };

  AddEnum(_("Label visibility"),
          _("Determines what labels are displayed."),
          wp_selection_list, (unsigned)settings.label_selection);
  SetExpertRow(WaypointLabelSelection);

  static constexpr StaticEnumChoice wp_label_style_list[] = {
    { WaypointRendererSettings::LabelStyle::TEXT,
      N_("Text"), N_("Plain text without an outline.") },
    { WaypointRendererSettings::LabelStyle::OUTLINED,
      N_("Outlined text"),
      N_("Black text with a white outline, readable on any background.") },
    { WaypointRendererSettings::LabelStyle::OUTLINED_INVERTED,
      N_("Inverted text"), N_("White text with a black outline.") },
    { WaypointRendererSettings::LabelStyle::BADGE,
      N_("Badge"), N_("Text on a rounded translucent background.") },
    nullptr
  };

  AddEnum(_("Label style"),
          _("How waypoint labels are drawn on the map."),
          wp_label_style_list, (unsigned)settings.label_style);
  SetExpertRow(WaypointTextStyle);

  static constexpr StaticEnumChoice wp_highlight_list[] = {
    { WaypointRendererSettings::HighlightStyle::NONE,
      N_("Same as others"), N_("Nothing is highlighted.") },
    { WaypointRendererSettings::HighlightStyle::BOLD,
      N_("Bold"), N_("The label style in bold.") },
    { WaypointRendererSettings::HighlightStyle::OUTLINED,
      N_("Outlined text"), N_("Bold black text with a white outline.") },
    { WaypointRendererSettings::HighlightStyle::OUTLINED_INVERTED,
      N_("Inverted text"), N_("Bold white text with a black outline.") },
    { WaypointRendererSettings::HighlightStyle::BADGE,
      N_("Badge"), N_("Bold text on a rounded translucent background.") },
    nullptr
  };

  AddEnum(_("Highlight"),
          _("How the labels of reachable landables, task waypoints and "
            "watched waypoints are drawn."),
          wp_highlight_list, (unsigned)settings.highlight_style);
  SetExpertRow(WaypointHighlightStyle);

  static constexpr StaticEnumChoice wp_arrival_info_list[] = {
    { WaypointRendererSettings::ArrivalInfo::NONE,
      N_("None"), N_("No arrival info is displayed.") },
    { WaypointRendererSettings::ArrivalInfo::ARRIVAL_HEIGHT,
      N_("Arrival height"),
      N_("The height above the safety arrival height at the waypoint.") },
    { WaypointRendererSettings::ArrivalInfo::GLIDE_RATIO,
      N_("Required glide ratio"),
      N_("The glide ratio over ground required to get there, as a whole "
         "number.") },
    { WaypointRendererSettings::ArrivalInfo::BOTH,
      N_("Both"),
      N_("The arrival height followed by the required glide ratio.") },
    nullptr
  };

  AddEnum(_("Arrival info"),
          _("Which value is displayed with the waypoint label."),
          wp_arrival_info_list, (unsigned)settings.arrival_info, this);
  SetExpertRow(WaypointArrivalInfo);

  static constexpr StaticEnumChoice wp_arrival_calculation_list[] = {
    { WaypointRendererSettings::ArrivalCalculation::STRAIGHT,
      N_("Straight glide"),
      N_("Straight glide arrival height (no terrain is considered).") },
    { WaypointRendererSettings::ArrivalCalculation::TERRAIN,
      N_("Terrain avoidance glide"),
      N_("Arrival height considering terrain avoidance. "
         "Requires \"Reach mode: Turning\" in \"Glide Computer > Route\" settings.") },
    { WaypointRendererSettings::ArrivalCalculation::BOTH,
      N_("Straight & terrain glide"),
      N_("Both arrival heights, but only where the detour costs at least "
         "10 m and 5 %. "
         "Requires \"Reach mode: Turning\" in \"Glide Computer > Route\" settings.") },
    nullptr
  };

  AddEnum(_("Calculation"),
          _("How the arrival height is calculated. The required glide ratio "
            "is always calculated straight over ground."),
          wp_arrival_calculation_list, (unsigned)settings.arrival_calculation);
  SetExpertRow(WaypointArrivalCalculation);

  static constexpr StaticEnumChoice wp_arrival_position_list[] = {
    { WaypointRendererSettings::ArrivalInfoPosition::AFTER_NAME,
      N_("After the name"),
      N_("Appended to the waypoint label, separated by a colon.") },
    { WaypointRendererSettings::ArrivalInfoPosition::BADGE_BELOW,
      N_("Badge below"),
      N_("On a rounded label of its own below the waypoint label.") },
    nullptr
  };

  AddEnum(_("Arrival info position"), nullptr,
          wp_arrival_position_list,
          (unsigned)settings.arrival_info_position);
  SetExpertRow(WaypointArrivalInfoPosition);

  static constexpr StaticEnumChoice wp_arrival_visibility_list[] = {
    { WaypointRendererSettings::ArrivalInfoVisibility::REACHABLE,
      N_("Reachable only"),
      N_("Only waypoints which can be reached, plus the watched ones.") },
    { WaypointRendererSettings::ArrivalInfoVisibility::ALL,
      N_("All labelled waypoints"),
      N_("Every waypoint which has a label, including unreachable ones.") },
    nullptr
  };

  AddEnum(_("Show arrival info"), nullptr,
          wp_arrival_visibility_list,
          (unsigned)settings.arrival_info_visibility);
  SetExpertRow(WaypointArrivalInfoVisibility);

  static constexpr StaticEnumChoice wp_style_list[] = {
    { WaypointRendererSettings::LandableStyle::PURPLE_CIRCLE,
      N_("Purple circle"),
      N_("Airports and outlanding fields are displayed as purple circles. If the waypoint is "
          "reachable a bigger green circle is added behind the purple one. If the waypoint is "
          "blocked by a mountain the green circle will be red instead.") },
    { WaypointRendererSettings::LandableStyle::BW,
      N_("B/W"),
      N_("Airports and outlanding fields are displayed in white/grey. If the waypoint is "
          "reachable the color is changed to green. If the waypoint is blocked by a mountain "
          "the color is changed to red instead.") },
    { WaypointRendererSettings::LandableStyle::TRAFFIC_LIGHTS,
      N_("Traffic lights"),
      N_("Airports and outlanding fields are displayed in the colors of a traffic light. "
          "Green if reachable, Orange if blocked by mountain and red if not reachable at all.") },
    nullptr
  };
  AddEnum(_("Landable symbols"),
          _("Three styles are available: Purple circles (WinPilot style), a high "
              "contrast (monochrome) style, or orange. The rendering differs for landable "
              "field and airport. All styles mark the waypoints within reach green."),
          wp_style_list, (unsigned)settings.landable_style);

  AddInteger(_("Waypoint icon size"),
             _("Size of waypoint symbols on the map as a percentage of the "
               "built-in artwork (list dialogs keep a fixed row icon size)."),
             "%u %%", "%u", 50, 200, 10, settings.map_waypoint_icon_scale);

  AddBoolean(_("Detailed landables"),
             _("[Off] Display fixed icons for landables.\n"
                 "[On] Show landables with variable information like runway length and heading."),
             settings.vector_landable_rendering, this);
  SetExpertRow(AppUseSWLandablesRendering);

  AddInteger(_("Landable size"),
             _("A percentage to select the size landables are displayed on the map."),
             "%u %%", "%u", 50, 200, 10, settings.landable_rendering_scale);
  SetExpertRow(AppLandableRenderingScale);

  AddBoolean(_("Scale runway length"),
             _("[Off] Display fixed length for runways.\n"
                 "[On] Scale displayed runway length based on real length."),
             settings.scale_runway_length);
  SetExpertRow(AppScaleRunwayLength);

  UpdateVisibilities();
}

bool
WaypointDisplayConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

  WaypointRendererSettings &settings = CommonInterface::SetMapSettings().waypoint;

  changed |= SaveValueEnum(WaypointLabels, ProfileKeys::DisplayText, settings.display_text_type);

  changed |= SaveValueEnum(WaypointLabelSelection, ProfileKeys::WaypointLabelSelection,
                           settings.label_selection);

  changed |= SaveValueEnum(WaypointTextStyle, ProfileKeys::WaypointTextStyle,
                           settings.label_style);

  changed |= SaveValueEnum(WaypointHighlightStyle,
                           ProfileKeys::WaypointHighlightStyle,
                           settings.highlight_style);

  changed |= SaveValueEnum(WaypointArrivalInfo,
                           ProfileKeys::WaypointArrivalInfo,
                           settings.arrival_info);

  changed |= SaveValueEnum(WaypointArrivalCalculation,
                           ProfileKeys::WaypointArrivalCalculation,
                           settings.arrival_calculation);

  changed |= SaveValueEnum(WaypointArrivalInfoPosition,
                           ProfileKeys::WaypointArrivalInfoPosition,
                           settings.arrival_info_position);

  changed |= SaveValueEnum(WaypointArrivalInfoVisibility,
                           ProfileKeys::WaypointArrivalInfoVisibility,
                           settings.arrival_info_visibility);

  changed |= SaveValueEnum(AppIndLandable, ProfileKeys::AppIndLandable, settings.landable_style);

  changed |= SaveValueInteger(MapWaypointIconScale, ProfileKeys::MapWaypointIconScale,
                              settings.map_waypoint_icon_scale);

  changed |= SaveValue(AppUseSWLandablesRendering, ProfileKeys::AppUseSWLandablesRendering,
                       settings.vector_landable_rendering);

  changed |= SaveValueInteger(AppLandableRenderingScale, ProfileKeys::AppLandableRenderingScale,
                              settings.landable_rendering_scale);

  changed |= SaveValue(AppScaleRunwayLength, ProfileKeys::AppScaleRunwayLength,
                       settings.scale_runway_length);

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateWaypointDisplayConfigPanel()
{
  return std::make_unique<WaypointDisplayConfigPanel>();
}
