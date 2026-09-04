// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WaypointRenderer.hpp"
#include "Renderer/LabelShape.hpp"
#include "Renderer/MapWaypointDrawLimits.hpp"
#include "WaypointRendererSettings.hpp"
#include "WaypointIconRenderer.hpp"
#include "WaypointLabelList.hpp"
#include "Projection/MapWindowProjection.hpp"
#include "Computer/Settings.hpp"
#include "Task/Visitors/TaskPointVisitor.hpp"
#include "Engine/Util/Gradient.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Engine/Waypoint/Waypoints.hpp"
#include "Engine/GlideSolvers/GlideState.hpp"
#include "Engine/GlideSolvers/GlideResult.hpp"
#include "Engine/GlideSolvers/MacCready.hpp"
#include "Engine/Task/TaskManager.hpp"
#include "Engine/Task/AbstractTask.hpp"
#include "Engine/Task/Unordered/UnorderedTaskPoint.hpp"
#include "Engine/Task/Ordered/Points/OrderedTaskPoint.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Task/ProtectedRoutePlanner.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Units/Units.hpp"
#include "util/StringFormat.hpp"
#include "util/TruncateString.hpp"
#include "util/StaticArray.hxx"
#include "util/Macros.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Engine/Route/ReachResult.hpp"
#include "Look/WaypointLook.hpp"

#include <cassert>
#include <math.h>
#include <stdio.h>

WaypointReach
CalculateWaypointReachRoute(const Waypoint &waypoint,
                            const ProtectedRoutePlanner &route_planner,
                            const TaskBehaviour &task_behaviour) noexcept
{
  WaypointReach reach;

  if (!waypoint.has_elevation)
    return reach;

  const double elevation = waypoint.elevation +
    task_behaviour.safety_height_arrival;
  const AGeoPoint p_dest(waypoint.location, elevation);

  const auto result = route_planner.FindPositiveArrival(p_dest);
  if (!result)
    return reach;

  reach.result = *result;
  reach.result.Subtract(elevation);

  if (!reach.result.IsReachableDirect())
    reach.reachability = WaypointReachability::UNREACHABLE;
  else if (task_behaviour.route_planner.IsReachEnabled() &&
           !reach.result.IsReachableTerrain())
    reach.reachability = WaypointReachability::STRAIGHT;
  else
    reach.reachability = WaypointReachability::TERRAIN;

  return reach;
}

WaypointReach
CalculateWaypointReachDirect(const Waypoint &waypoint, const MoreData &basic,
                             const SpeedVector &wind,
                             const MacCready &mac_cready,
                             const TaskBehaviour &task_behaviour) noexcept
{
  assert(basic.location_available);
  assert(basic.NavAltitudeAvailable());

  WaypointReach reach;

  if (!waypoint.has_elevation)
    return reach;

  const auto elevation = waypoint.elevation +
    task_behaviour.safety_height_arrival;
  const GlideState state(GeoVector(basic.location, waypoint.location),
                         elevation, basic.nav_altitude, wind);

  const GlideResult result = mac_cready.SolveStraight(state);
  if (!result.IsOk())
    return reach;

  reach.result.direct = result.pure_glide_altitude_difference;
  reach.reachability = result.pure_glide_altitude_difference > 0
    ? WaypointReachability::TERRAIN
    : WaypointReachability::UNREACHABLE;

  return reach;
}

WaypointReach
CalculateWaypointReach(const Waypoint &waypoint,
                       const ProtectedRoutePlanner *route_planner,
                       const MoreData &basic, const DerivedInfo &calculated,
                       const PolarSettings &polar_settings,
                       const TaskBehaviour &task_behaviour) noexcept
{
  if (route_planner != nullptr && !route_planner->IsTerrainReachEmpty())
    return CalculateWaypointReachRoute(waypoint, *route_planner,
                                       task_behaviour);

  if (!basic.location_available || !basic.NavAltitudeAvailable())
    return {};

  const GlidePolar &glide_polar =
    task_behaviour.route_planner.reach_polar_mode == RoutePlannerConfig::Polar::TASK
    ? polar_settings.glide_polar_task
    : calculated.glide_polar_safety;

  return CalculateWaypointReachDirect(waypoint, basic,
                                      calculated.GetWindOrZero(),
                                      MacCready(task_behaviour.glide,
                                                glide_polar),
                                      task_behaviour);
}

/**
 * Metadata for a Waypoint that is about to be drawn.
 */
struct VisibleWaypoint {
  WaypointPtr waypoint;

  PixelPoint point;

  ReachResult reach;

  WaypointReachability reachable;

  bool in_task;

  void Set(const WaypointPtr &_waypoint, PixelPoint &_point,
           bool _in_task) noexcept {
    waypoint = _waypoint;
    point = _point;
    reach.Clear();
    reachable = WaypointReachability::INVALID;
    in_task = _in_task;
  }

  bool IsReachable() const noexcept {
    return ::IsReachable(reachable);
  }

  void Set(const WaypointReach &_reach) noexcept {
    reach = _reach.result;
    reachable = _reach.reachability;
  }

  void CalculateReachabilityDirect(const MoreData &basic,
                                   const SpeedVector &wind,
                                   const MacCready &mac_cready,
                                   const TaskBehaviour &task_behaviour) noexcept {
    Set(CalculateWaypointReachDirect(*waypoint, basic, wind, mac_cready,
                                     task_behaviour));
  }

  void CalculateReachability(const ProtectedRoutePlanner &route_planner,
                             const TaskBehaviour &task_behaviour) noexcept
  {
    Set(CalculateWaypointReachRoute(*waypoint, route_planner, task_behaviour));
  }

  void DrawSymbol(WaypointIconRenderer &wir) const noexcept {
    wir.Draw(*waypoint, point, reachable,
             in_task);
  }
};

class WaypointVisitorMap final
  : public TaskPointConstVisitor
{
  const MapWindowProjection &projection;
  const WaypointRendererSettings &settings;
  const WaypointLook &look;
  const TaskBehaviour &task_behaviour;
  const MoreData &basic;

  char altitude_unit[4];
  bool task_valid;

  /**
   * A list of waypoints that are going to be drawn.  This list is
   * filled in the Visitor methods.  In the second stage, their
   * reachability is calculated, and the third stage draws them.  This
   * should ensure that the drawing methods don't need to hold a
   * mutex.
   */
  StaticArray<VisibleWaypoint, MAX_MAP_WAYPOINT_DRAW> waypoints;

  WaypointIconRenderer icon_renderer;

public:
  WaypointLabelList labels;

public:
  WaypointVisitorMap(Canvas &_canvas,
                     const MapWindowProjection &_projection,
                     const WaypointRendererSettings &_settings,
                     const WaypointLook &_look,
                     const TaskBehaviour &_task_behaviour,
                     const MoreData &_basic) noexcept
    :projection(_projection),
     settings(_settings), look(_look), task_behaviour(_task_behaviour),
     basic(_basic),
     task_valid(false),
     icon_renderer(settings, look,
                   _canvas,
                   projection.GetMapScale() > 4000,
                   projection.GetScreenAngle()),
     labels(projection.GetScreenRect())
  {
    strcpy(altitude_unit, Units::GetAltitudeName());
  }


protected:
  void FormatTitle(char *buffer, size_t buffer_size,
                   const Waypoint &way_point) const noexcept {
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
      buffer[0] = '\0';
      break;

    case WaypointRendererSettings::DisplayTextType::FIRST_WORD:
      CopyTruncateString(buffer, buffer_size, way_point.name.c_str());
      char *tmp;
      tmp = strstr(buffer, " ");
      if (tmp != nullptr)
        tmp[0] = '\0';
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
   * Format the arrival height of the waypoint, according to the
   * configured calculation.
   *
   * @return false if no arrival height is available
   */
  bool FormatArrivalHeight(char *buffer, size_t buffer_size,
                           WaypointReachability reachable,
                           const ReachResult &reach) const noexcept {
    if (reachable == WaypointReachability::INVALID)
      return false;

    const int uah_glide = (int)Units::ToUserAltitude(reach.direct);
    const int uah_terrain = (int)Units::ToUserAltitude(reach.terrain);

    switch (settings.arrival_calculation) {
    case WaypointRendererSettings::ArrivalCalculation::TERRAIN:
      if (!reach.IsReachableTerrain())
        return false;

      StringFormat(buffer, buffer_size, "%d%s", uah_terrain, altitude_unit);
      return true;

    case WaypointRendererSettings::ArrivalCalculation::BOTH:
      /* two values are only worth the space if the detour really
         costs something */
      if (reach.IsReachableDirect() && reach.IsReachableTerrain() &&
          reach.IsDeltaConsiderable()) {
        StringFormat(buffer, buffer_size, "%d/%d%s", uah_glide,
                     uah_terrain, altitude_unit);
        return true;
      }

      break;

    case WaypointRendererSettings::ArrivalCalculation::STRAIGHT:
      break;
    }

    StringFormat(buffer, buffer_size, "%d%s", uah_glide, altitude_unit);
    return true;
  }

  /**
   * Format the glide ratio over ground which is required to reach the
   * waypoint at the configured arrival safety height.
   *
   * @return false if no glide ratio is available
   */
  bool FormatRequiredGlideRatio(char *buffer, size_t buffer_size,
                                const Waypoint &way_point) const noexcept {
    if (!basic.location_available || !basic.NavAltitudeAvailable() ||
        !way_point.has_elevation)
      return false;

    const auto safety_height = task_behaviour.safety_height_arrival;
    const auto target_altitude = way_point.elevation + safety_height;
    const auto delta_h = basic.nav_altitude - target_altitude;
    if (delta_h <= 0)
      /* no glide ratio if below the waypoint */
      return false;

    const auto distance = basic.location.DistanceS(way_point.location);
    const auto gr = distance / delta_h;
    if (!GradientValid(gr))
      return false;

    StringFormat(buffer, buffer_size, "%d", (int)lround(gr));
    return true;
  }

  /**
   * Is this waypoint reachable enough to be worth arrival info?
   */
  bool HasArrivalInfo(const Waypoint &way_point,
                      WaypointReachability reachable,
                      const ReachResult &reach) const noexcept {
    if (settings.arrival_info_visibility ==
        WaypointRendererSettings::ArrivalInfoVisibility::ALL)
      return true;

    return reachable != WaypointReachability::INVALID &&
      (reach.IsReachableDirect() || way_point.flags.watched);
  }

  /**
   * Format the arrival info which is drawn with the waypoint label.
   *
   * @return false if there is nothing to draw
   */
  bool FormatArrivalInfo(char *buffer, size_t buffer_size,
                         const Waypoint &way_point,
                         WaypointReachability reachable,
                         const ReachResult &reach) const noexcept {
    buffer[0] = '\0';

    if (settings.arrival_info == WaypointRendererSettings::ArrivalInfo::NONE)
      return false;

    if (!HasArrivalInfo(way_point, reachable, reach))
      return false;

    size_t length = 0;

    if (settings.arrival_info != WaypointRendererSettings::ArrivalInfo::GLIDE_RATIO &&
        FormatArrivalHeight(buffer, buffer_size, reachable, reach))
      length = strlen(buffer);

    if (settings.arrival_info != WaypointRendererSettings::ArrivalInfo::ARRIVAL_HEIGHT) {
      if (length > 0 && length + 1 < buffer_size)
        buffer[length++] = ' ';

      if (!FormatRequiredGlideRatio(buffer + length, buffer_size - length,
                                    way_point))
        buffer[length] = '\0';
    }

    return buffer[0] != '\0';
  }

  /**
   * Is this waypoint drawn with the highlight style?
   */
  bool IsHighlighted(const VisibleWaypoint &vwp) const noexcept {
    return (vwp.IsReachable() && vwp.waypoint->IsLandable()) ||
      vwp.in_task || vwp.waypoint->flags.watched;
  }

  [[gnu::pure]]
  static LabelShape ToLabelShape(WaypointRendererSettings::LabelStyle style) noexcept {
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

  void DrawWaypoint(const VisibleWaypoint &vwp) noexcept {
    const Waypoint &way_point = *vwp.waypoint;
    bool watchedWaypoint = way_point.flags.watched;

    vwp.DrawSymbol(icon_renderer);

    // Determine whether to draw the waypoint label or not
    switch (settings.label_selection) {
    case WaypointRendererSettings::LabelSelection::NONE:
      return;

    case WaypointRendererSettings::LabelSelection::TASK:
      if (!vwp.in_task && task_valid && !watchedWaypoint)
        return;
      break;

    case WaypointRendererSettings::LabelSelection::TASK_AND_AIRFIELD:
      if (!vwp.in_task && task_valid && !watchedWaypoint &&
          !way_point.IsAirport())
        return;
      break;

    case WaypointRendererSettings::LabelSelection::TASK_AND_LANDABLE:
      if (!vwp.in_task && task_valid && !watchedWaypoint &&
          !way_point.IsLandable())
        return;
      break;

    default:
      break;
    }

    TextInBoxMode text_mode;
    text_mode.shape = ToLabelShape(settings.label_style);
    bool bold = false;

    if (settings.highlight_style !=
        WaypointRendererSettings::HighlightStyle::NONE && IsHighlighted(vwp)) {
      text_mode.move_in_view = true;
      bold = true;

      switch (settings.highlight_style) {
      case WaypointRendererSettings::HighlightStyle::OUTLINED:
        text_mode.shape = LabelShape::OUTLINED;
        break;

      case WaypointRendererSettings::HighlightStyle::OUTLINED_INVERTED:
        text_mode.shape = LabelShape::OUTLINED_INVERTED;
        break;

      case WaypointRendererSettings::HighlightStyle::BADGE:
        text_mode.shape = LabelShape::ROUNDED_WHITE;
        break;

      case WaypointRendererSettings::HighlightStyle::NONE:
      case WaypointRendererSettings::HighlightStyle::BOLD:
        break;
      }
    }

    char buffer[NAME_SIZE+1];
    FormatTitle(buffer, ARRAY_SIZE(buffer) - 20, way_point);

    char info[16];
    const bool has_info = FormatArrivalInfo(info, ARRAY_SIZE(info),
                                            way_point, vwp.reachable,
                                            vwp.reach);

    const bool info_below = has_info &&
      settings.arrival_info_position ==
      WaypointRendererSettings::ArrivalInfoPosition::BADGE_BELOW;

    if (has_info && !info_below) {
      size_t length = strlen(buffer);
      if (length > 0)
        buffer[length++] = ':';

      CopyTruncateString(buffer + length, ARRAY_SIZE(buffer) - length, info);
    }

    auto sc = vwp.point;
    sc.x += 5;
    if ((vwp.IsReachable() &&
         settings.landable_style == WaypointRendererSettings::LandableStyle::PURPLE_CIRCLE) ||
        settings.vector_landable_rendering)
      // make space for the green circle
      sc.x += 5;

    const int arrival_agl = vwp.reachable != WaypointReachability::INVALID
      ? vwp.reach.direct
      : INT_MIN;

    labels.Add(buffer, sc, text_mode, bold, arrival_agl,
               vwp.in_task, way_point.IsLandable(), way_point.IsAirport(),
               watchedWaypoint, false);

    if (info_below) {
      TextInBoxMode info_mode;
      info_mode.shape = LabelShape::ROUNDED_WHITE;
      info_mode.move_in_view = true;

      labels.Add(info, sc, info_mode, false, arrival_agl,
                 vwp.in_task, way_point.IsLandable(), way_point.IsAirport(),
                 watchedWaypoint, true);
    }
  }

  void AddWaypoint(const WaypointPtr &way_point, bool in_task) noexcept {
    if (waypoints.full())
      return;

    if (!projection.WaypointInScaleFilter(*way_point) && !in_task)
      return;

    if (auto p = projection.GeoToScreenIfVisible(way_point->location)) {
      VisibleWaypoint &vwp = waypoints.append();
      vwp.Set(way_point, *p, in_task);
    }
  }

public:
  void Add(const WaypointPtr &way_point) noexcept {
    AddWaypoint(way_point, false);
  }

  void Visit(const TaskPoint &tp) override {
    switch (tp.GetType()) {
    case TaskPointType::UNORDERED:
      AddWaypoint(((const UnorderedTaskPoint &)tp).GetWaypointPtr(), true);
      break;

    case TaskPointType::START:
    case TaskPointType::AST:
    case TaskPointType::AAT:
    case TaskPointType::FINISH:
      AddWaypoint(((const OrderedTaskPoint &)tp).GetWaypointPtr(), true);
      break;
    }
  }

public:
  void SetTaskValid() noexcept {
    task_valid = true;
  }

  /**
   * Does the arrival height need to be calculated for waypoints which
   * are neither landable nor watched?
   */
  [[gnu::pure]]
  bool NeedsReachEverywhere() const noexcept {
    return settings.arrival_info_visibility ==
      WaypointRendererSettings::ArrivalInfoVisibility::ALL &&
      settings.arrival_info != WaypointRendererSettings::ArrivalInfo::NONE &&
      settings.arrival_info != WaypointRendererSettings::ArrivalInfo::GLIDE_RATIO;
  }

  [[gnu::pure]]
  bool NeedsReach(const Waypoint &way_point) const noexcept {
    return way_point.IsLandable() || way_point.flags.watched ||
      NeedsReachEverywhere();
  }

  void CalculateRoute(const ProtectedRoutePlanner &route_planner) noexcept {
    for (VisibleWaypoint &vwp : waypoints) {
      if (NeedsReach(*vwp.waypoint))
        vwp.CalculateReachability(route_planner, task_behaviour);
    }
  }

  void CalculateDirect(const PolarSettings &polar_settings,
                       const TaskBehaviour &task_behaviour,
                       const DerivedInfo &calculated) noexcept {
    if (!basic.location_available || !basic.NavAltitudeAvailable())
      return;

    const GlidePolar &glide_polar =
      task_behaviour.route_planner.reach_polar_mode == RoutePlannerConfig::Polar::TASK
      ? polar_settings.glide_polar_task
      : calculated.glide_polar_safety;
    const MacCready mac_cready(task_behaviour.glide, glide_polar);

    for (VisibleWaypoint &vwp : waypoints) {
      if (NeedsReach(*vwp.waypoint))
        vwp.CalculateReachabilityDirect(basic, calculated.GetWindOrZero(),
                                        mac_cready, task_behaviour);
    }
  }

  void Calculate(const ProtectedRoutePlanner *route_planner,
                 const PolarSettings &polar_settings,
                 const TaskBehaviour &task_behaviour,
                 const DerivedInfo &calculated) noexcept {
    if (route_planner != nullptr && !route_planner->IsTerrainReachEmpty())
      CalculateRoute(*route_planner);
    else
      CalculateDirect(polar_settings, task_behaviour, calculated);
  }

  void Draw() noexcept {
    for (const VisibleWaypoint &vwp : waypoints)
      DrawWaypoint(vwp);
  }
};

static void
MapWaypointLabelRender(Canvas &canvas, PixelSize clip_size,
                       LabelBlock &label_block,
                       WaypointLabelList &labels,
                       const WaypointLook &look) noexcept
{
  labels.Sort();

  for (const auto &l : labels) {
    canvas.Select(l.bold ? *look.bold_font : *look.font);

    /* the arrival info badge goes below the waypoint label */
    const int offset = l.isArrivalInfo
      ? (int)(canvas.GetFontHeight() + Layout::GetTextPadding())
      : 0;

    TextInBox(canvas, l.Name, l.Pos.At(0, offset), l.Mode, clip_size,
              &label_block);
  }
}

void
WaypointRenderer::Render(Canvas &canvas, LabelBlock &label_block,
                         const MapWindowProjection &projection,
                         const struct WaypointRendererSettings &settings,
                         const PolarSettings &polar_settings,
                         const TaskBehaviour &task_behaviour,
                         const MoreData &basic, const DerivedInfo &calculated,
                         const ProtectedTaskManager *task,
                         const ProtectedRoutePlanner *route_planner) noexcept
{
  if (way_points == nullptr || way_points->IsEmpty())
    return;

  WaypointVisitorMap v(canvas, projection, settings, look, task_behaviour, basic);

  if (task != nullptr) {
    ProtectedTaskManager::Lease task_manager(*task);

    const TaskStats &task_stats = task_manager->GetStats();

    // task items come first, this is the only way we know that an item is in task,
    // and we won't add it if it is already there
    if (task_stats.task_valid)
      v.SetTaskValid();

    const AbstractTask *atask = task_manager->GetActiveTask();
    if (atask != nullptr)
      atask->AcceptTaskPointVisitor(v);
  }

  way_points->VisitWithinRange(projection.GetGeoScreenCenter(),
                               projection.GetScreenDistanceMeters(),
                               [&v](const auto &w){ v.Add(w); });

  v.Calculate(route_planner, polar_settings, task_behaviour, calculated);

  v.Draw();

  MapWaypointLabelRender(canvas, projection.GetScreenSize(),
                         label_block, v.labels, look);
}
