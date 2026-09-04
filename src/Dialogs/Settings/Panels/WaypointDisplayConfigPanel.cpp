// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WaypointDisplayConfigPanel.hpp"
#include "Profile/Keys.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Listener.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Look/DialogLook.hpp"
#include "Look/Look.hpp"
#include "Look/MapLook.hpp"
#include "MainWindow.hpp"
#include "Look/WaypointLook.hpp"
#include "Renderer/LabelBlock.hpp"
#include "Renderer/TextInBox.hpp"
#include "Renderer/WaypointIconRenderer.hpp"
#include "Renderer/WaypointLabelFormat.hpp"
#include "Screen/Layout.hpp"
#include "Units/Units.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Widget/WindowWidget.hpp"
#include "UIGlobals.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/window/ContainerWindow.hpp"
#include "ui/window/PaintWindow.hpp"
#include "util/Macros.hpp"
#include "util/TruncateString.hpp"

#include <memory>
#include <span>
#include <string.h>
#include <vector>

/**
 * A preview of a handful of waypoint labels on a schematic map, so
 * the label settings can be judged without leaving the dialog.
 */
class WaypointPreviewWindow final : public PaintWindow {
  struct Sample {
    Waypoint waypoint;

    /** relative position in the preview */
    double x, y;

    /** made up values; the preview shows the layout, not a solution */
    WaypointArrivalValues values;

    bool reachable;
  };

  WaypointRendererSettings settings;

  /**
   * The preview needs the icons of the style which is selected in the
   * dialog, not the one the map currently uses.
   */
  WaypointLook look;

  std::vector<Sample> samples;

public:
  explicit WaypointPreviewWindow(const WaypointRendererSettings &_settings)
    noexcept
    :settings(_settings)
  {
    const auto &map_look = UIGlobals::GetMapLook().waypoint;
    look.Initialise(settings, *map_look.font, *map_look.bold_font);

    samples.reserve(8);

    Runway runway = Runway::Null();
    runway.SetDirectionDegrees(160);
    runway.SetLength(1000);

    /* Freiburg in the middle, the others roughly where they are from
       there */
    auto &freiburg = Add("Freiburg", "EDTF", Waypoint::Type::AIRFIELD,
                         0.42, 0.46, true);
    freiburg.waypoint.runway = runway;
    freiburg.values.height_straight = (int)Units::ToUserAltitude(450);
    freiburg.values.height_terrain = (int)Units::ToUserAltitude(380);
    freiburg.values.considerable_delta = true;
    freiburg.values.glide_ratio = 24;

    /* north west, the Kaiserstuhl */
    auto &kaiserstuhl = Add("Totenkopf", nullptr,
                            Waypoint::Type::MOUNTAIN_TOP, 0.12, 0.16, false);
    kaiserstuhl.values.height_straight = (int)Units::ToUserAltitude(210);
    kaiserstuhl.values.glide_ratio = 18;

    /* east south east, a glider site */
    auto &kirchzarten = Add("Kirchzarten", nullptr,
                            Waypoint::Type::AIRFIELD, 0.60, 0.52, false);
    Runway kirchzarten_runway = Runway::Null();
    kirchzarten_runway.SetDirectionDegrees(70);
    kirchzarten_runway.SetLength(600);
    kirchzarten.waypoint.runway = kirchzarten_runway;
    kirchzarten.values.height_straight = (int)Units::ToUserAltitude(120);
    kirchzarten.values.glide_ratio = 27;

    /* south south east, the Feldberg */
    auto &feldberg = Add("Feldberg", nullptr,
                         Waypoint::Type::MOUNTAIN_TOP, 0.46, 0.74, false);
    feldberg.values.height_straight = (int)Units::ToUserAltitude(-180);
    feldberg.values.glide_ratio = 62;

    /* east south east, an outlanding field */
    auto &loeffingen = Add("Löffingen", nullptr,
                           Waypoint::Type::OUTLANDING, 0.72, 0.70, false);
    loeffingen.values.height_straight = (int)Units::ToUserAltitude(-40);
    loeffingen.values.glide_ratio = 38;

    /* south, in the Hotzenwald */
    auto &huetten = Add("Hütten Hotzenwald", "EDXX",
                        Waypoint::Type::AIRFIELD, 0.30, 0.86, false);
    Runway huetten_runway = Runway::Null();
    huetten_runway.SetDirectionDegrees(120);
    huetten_runway.SetLength(700);
    huetten.waypoint.runway = huetten_runway;
    huetten.values.height_straight = (int)Units::ToUserAltitude(-90);
    huetten.values.glide_ratio = 41;
  }

  void SetSettings(const WaypointRendererSettings &_settings) noexcept {
    if (_settings.landable_style != settings.landable_style)
      look.Reinitialise(_settings);

    settings = _settings;
    Invalidate();
  }

protected:
  Sample &Add(const char *name, const char *shortname,
              Waypoint::Type type, double x, double y,
              bool reachable) noexcept {
    auto &sample = samples.emplace_back(Sample{Waypoint{GeoPoint::Zero()},
                                              x, y, {}, reachable});
    sample.waypoint.name = name;
    if (shortname != nullptr)
      sample.waypoint.shortname = shortname;
    sample.waypoint.type = type;
    sample.waypoint.elevation = 400;
    sample.waypoint.has_elevation = true;
    return sample;
  }

  /** keeps the labels from overlapping, just like on the map */
  LabelBlock label_block;

  void DrawSample(Canvas &canvas, const PixelRect &rc,
                  const Sample &sample) noexcept;

  /* virtual methods from class PaintWindow */
  void OnPaint(Canvas &canvas) noexcept override;
};

/**
 * The row which holds the preview.  It asks for more height than a
 * control row, which is what makes the page scroll.
 */
class WaypointPreviewWidget final : public WindowWidget {
  const WaypointRendererSettings &settings;

public:
  explicit WaypointPreviewWidget(const WaypointRendererSettings &_settings)
    noexcept
    :settings(_settings) {}

  void SetSettings(const WaypointRendererSettings &_settings) noexcept {
    if (IsDefined())
      ((WaypointPreviewWindow &)GetWindow()).SetSettings(_settings);
  }

  /* virtual methods from class Widget */
  PixelSize GetMinimumSize() const noexcept override {
    return {0u, Layout::GetMaximumControlHeight() * 5};
  }

  PixelSize GetMaximumSize() const noexcept override {
    return {0u, Layout::GetMaximumControlHeight() * 7};
  }

  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override {
    WindowStyle style;
    style.Hide();
    style.Border();

    /* Window::IsDefined() is false for a zero width, and the row
       position may still be empty at this point */
    PixelRect safe_rc = rc;
    if (safe_rc.GetWidth() == 0)
      safe_rc.right = safe_rc.left + 1;
    if (safe_rc.GetHeight() == 0)
      safe_rc.bottom = safe_rc.top + 1;

    auto window = std::make_unique<WaypointPreviewWindow>(settings);
    window->Create(parent, safe_rc, style);
    SetWindow(std::move(window));
  }
};

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

  WaypointPreviewWidget *preview = nullptr;

public:
  WaypointDisplayConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  void UpdateVisibilities();
  void UpdatePreview();

  /* methods from Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
};

/**
 * Split a label into the segments which are drawn side by side.
 */
static std::size_t
SplitSegments(char *text, std::span<const char *> segments) noexcept
{
  std::size_t n = 0;
  char *p = text;
  segments[n++] = p;

  while (n < segments.size()) {
    char *separator = strchr(p, '\n');
    if (separator == nullptr)
      break;

    *separator = '\0';
    p = separator + 1;
    segments[n++] = p;
  }

  return n;
}

void
WaypointPreviewWindow::DrawSample(Canvas &canvas, const PixelRect &rc,
                                  const Sample &sample) noexcept
{
  const Waypoint &way_point = sample.waypoint;

  const PixelPoint p(rc.left + (int)(rc.GetWidth() * sample.x),
                     rc.top + (int)(rc.GetHeight() * sample.y));

  const auto reachability = sample.reachable
    ? WaypointReachability::TERRAIN
    : WaypointReachability::UNREACHABLE;

  WaypointIconRenderer icon_renderer(settings, look, canvas);
  icon_renderer.Draw(way_point, p, reachability);

  /* a reachable landable is what the highlight style is for */
  const auto appearance = GetWaypointLabelAppearance(settings,
                                                     sample.reachable);

  char buffer[64];
  FormatWaypointLabelTitle(buffer, ARRAY_SIZE(buffer) - 20, settings,
                           way_point);

  char info[16];
  info[0] = '\0';

  /* without a glide solution, an unreachable waypoint gets its value
     only in the "all labelled waypoints" mode, just like on the map */
  if (sample.reachable ||
      settings.arrival_info_visibility ==
      WaypointRendererSettings::ArrivalInfoVisibility::ALL)
    FormatWaypointArrivalInfo(info, ARRAY_SIZE(info), settings,
                              sample.values, Units::GetAltitudeName());

  const bool info_below = info[0] != '\0' &&
    settings.arrival_info_position ==
    WaypointRendererSettings::ArrivalInfoPosition::BADGE_BELOW;

  TextInBoxMode mode;
  mode.shape = appearance.shape;

  if (info[0] != '\0' && !info_below) {
    size_t length = strlen(buffer);
    if (length > 0)
      buffer[length++] = '\n';

    CopyTruncateString(buffer + length, ARRAY_SIZE(buffer) - length, info);

    mode.compact = true;
  }

  const PixelSize size = canvas.GetSize();
  const auto label_pos = p.At(Layout::Scale(10), 0);

  canvas.Select(appearance.bold ? *look.bold_font : *look.font);

  const char *segments[3];
  std::size_t n = SplitSegments(buffer, segments);
  TextInBox(canvas, std::span{segments}.first(n), label_pos, mode, size,
            &label_block);

  if (info_below) {
    TextInBoxMode info_mode;
    info_mode.shape = LabelShape::ROUNDED_WHITE;
    info_mode.compact = true;

    canvas.Select(*look.font);

    n = SplitSegments(info, segments);
    TextInBox(canvas, std::span{segments}.first(n),
              label_pos.At(0, canvas.GetFontHeight() +
                           Layout::GetTextPadding()),
              info_mode, size, &label_block);
  }
}

void
WaypointPreviewWindow::OnPaint(Canvas &canvas) noexcept
{
  const PixelRect rc = canvas.GetRect();

  /* a schematic map: a plain background would not show what the
     outline and the badge are for.  These are fixed, muted versions
     of the terrain ramp */
  static constexpr Color terrain[] = {
    Color(0xa7, 0xbc, 0x94),
    Color(0xc4, 0xc7, 0x97),
    Color(0xd6, 0xbb, 0x8f),
    Color(0xc2, 0x9b, 0x7e),
  };

  const int height = (int)rc.GetHeight();

  for (unsigned i = 0; i < ARRAY_SIZE(terrain); ++i) {
    PixelRect band = rc;
    band.top = rc.top + height * int(i) / int(ARRAY_SIZE(terrain));
    band.bottom = rc.top + height * int(i + 1) / int(ARRAY_SIZE(terrain));
    canvas.DrawFilledRectangle(band, terrain[i]);
  }

  label_block.reset();

  for (const auto &sample : samples)
    DrawSample(canvas, rc, sample);
}

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
  SetRowEnabled(WaypointArrivalCalculation,
                WaypointRendererSettings::Contains(arrival_info,
                                                   WaypointRendererSettings::ArrivalInfo::HEIGHT) ||
                WaypointRendererSettings::Contains(arrival_info,
                                                   WaypointRendererSettings::ArrivalInfo::ALTITUDE));
  SetRowEnabled(WaypointArrivalInfoPosition, has_arrival_info);
  SetRowEnabled(WaypointArrivalInfoVisibility, has_arrival_info);
}

void
WaypointDisplayConfigPanel::UpdatePreview()
{
  if (preview == nullptr)
    return;

  WaypointRendererSettings s = CommonInterface::GetMapSettings().waypoint;

  s.display_text_type = (WaypointRendererSettings::DisplayTextType)
    GetValueEnum(WaypointLabels);
  s.label_style = (WaypointRendererSettings::LabelStyle)
    GetValueEnum(WaypointTextStyle);
  s.highlight_style = (WaypointRendererSettings::HighlightStyle)
    GetValueEnum(WaypointHighlightStyle);
  s.arrival_info = (WaypointRendererSettings::ArrivalInfo)
    GetValueEnum(WaypointArrivalInfo);
  s.arrival_calculation = (WaypointRendererSettings::ArrivalCalculation)
    GetValueEnum(WaypointArrivalCalculation);
  s.arrival_info_position = (WaypointRendererSettings::ArrivalInfoPosition)
    GetValueEnum(WaypointArrivalInfoPosition);
  s.arrival_info_visibility = (WaypointRendererSettings::ArrivalInfoVisibility)
    GetValueEnum(WaypointArrivalInfoVisibility);
  s.landable_style = (WaypointRendererSettings::LandableStyle)
    GetValueEnum(AppIndLandable);
  s.map_waypoint_icon_scale = GetValueInteger(MapWaypointIconScale);
  s.vector_landable_rendering = GetValueBoolean(AppUseSWLandablesRendering);
  s.landable_rendering_scale = GetValueInteger(AppLandableRenderingScale);
  s.scale_runway_length = GetValueBoolean(AppScaleRunwayLength);

  preview->SetSettings(s);
}

void
WaypointDisplayConfigPanel::OnModified(DataField &df) noexcept
{
  if (IsDataField(AppUseSWLandablesRendering, df) ||
      IsDataField(WaypointArrivalInfo, df))
    UpdateVisibilities();

  UpdatePreview();
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

  AddEnum(_("Reachable label style"),
          _("How the labels of reachable landables are drawn; task "
            "waypoints and watched waypoints get the same treatment."),
          wp_highlight_list, (unsigned)settings.highlight_style);
  SetExpertRow(WaypointHighlightStyle);

  static constexpr StaticEnumChoice wp_arrival_info_list[] = {
    { WaypointRendererSettings::ArrivalInfo::NONE,
      N_("None"), N_("No arrival info is displayed.") },
    { WaypointRendererSettings::ArrivalInfo::HEIGHT,
      N_("Arrival height"),
      N_("The height above the safety arrival height at the waypoint.") },
    { WaypointRendererSettings::ArrivalInfo::ALTITUDE,
      N_("Arrival altitude"),
      N_("The altitude at which the waypoint is reached, safety arrival "
         "height included.") },
    { WaypointRendererSettings::ArrivalInfo::GLIDE_RATIO,
      N_("Required glide ratio"),
      N_("The glide ratio over ground required to get there, as a whole "
         "number.") },
    { WaypointRendererSettings::ArrivalInfo::HEIGHT_AND_GLIDE_RATIO,
      N_("Height & glide ratio") },
    { WaypointRendererSettings::ArrivalInfo::ALTITUDE_AND_GLIDE_RATIO,
      N_("Altitude & glide ratio") },
    { WaypointRendererSettings::ArrivalInfo::HEIGHT_AND_ALTITUDE,
      N_("Height & altitude") },
    { WaypointRendererSettings::ArrivalInfo::ALL,
      N_("Height, altitude & glide ratio") },
    nullptr
  };

  AddEnum(_("Arrival info"),
          _("Which value is displayed with the waypoint label."),
          wp_arrival_info_list, (unsigned)settings.arrival_info);
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
      N_("Reachable landables"),
      N_("Only landables which can be reached, plus the watched "
         "waypoints.") },
    { WaypointRendererSettings::ArrivalInfoVisibility::LANDABLE,
      N_("All landables"),
      N_("All landables, whether they can be reached or not, plus the "
         "watched waypoints.") },
    { WaypointRendererSettings::ArrivalInfoVisibility::ALL,
      N_("All labelled waypoints"),
      N_("Every waypoint which has a label, including the ones which are "
         "not landable.") },
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
             settings.vector_landable_rendering);
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

  /* every row feeds the preview; the listener must not be set at Add()
     time as well, DataField::SetListener() allows only one */
  for (unsigned i = WaypointLabels; i <= AppScaleRunwayLength; ++i)
    GetDataField(i).SetListener(this);

  AddSpacer();

  auto preview_widget = std::make_unique<WaypointPreviewWidget>(settings);
  preview = preview_widget.get();
  Add(std::move(preview_widget));

  UpdateVisibilities();
  UpdatePreview();
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

  if (SaveValueEnum(AppIndLandable, ProfileKeys::AppIndLandable,
                    settings.landable_style)) {
    changed = true;

    /* the landable icons are loaded for one style only */
    auto &look = CommonInterface::main_window->SetLook().map.waypoint;
    look.Reinitialise(settings);
  }

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
