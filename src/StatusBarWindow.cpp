// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "StatusBarWindow.hpp"
#include "Interface.hpp"
#include "UIState.hpp"
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "Replay/Replay.hpp"
#include "NMEA/Info.hpp"
#include "Logger/Logger.hpp"
#include "Hardware/PowerGlobal.hpp"
#include "Hardware/PowerInfo.hpp"
#include "net/State.hpp"
#include "InfoBoxes/Content/Base.hpp"
#include "InfoBoxes/Content/Factory.hpp"
#include "InfoBoxes/Data.hpp"
#include "Units/Descriptor.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "Input/InputEvents.hpp"
#include "Dialogs/InternalLink.hpp"
#include "PageActions.hpp"
#include "Language/Language.hpp"
#include "Screen/Layout.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Font.hpp"
#include "Look/FontDescription.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Pen.hpp"
#include "Math/Angle.hpp"
#include "Asset.hpp"
#include "util/StaticString.hxx"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <iterator>

/**
 * Format the data source (simulator, replay).  Empty if the data
 * comes from the GPS.
 *
 * @param abbreviated "SIM" and "REP" where room is short
 */
static void
FormatDataSource(StaticString<32> &buffer, const NMEAInfo &basic,
                 bool abbreviated) noexcept
{
  buffer.clear();

  if (basic.gps.replay) {
    buffer = abbreviated ? C_("Abbreviation", "REP") : _("Replay");
    if (backend_components != nullptr &&
        backend_components->replay != nullptr)
      buffer.AppendFormat(" %.0fx",
                          backend_components->replay->GetTimeScale());
  } else if (basic.gps.simulator || DEBUG_ALL_MAP_OVERLAYS)
    buffer = abbreviated ? C_("Abbreviation", "SIM") : _("Simulator");
}

StatusBarWindow::StatusBarWindow(const Font &_font,
                                 UISettings::StatusBarStyle _style) noexcept
  :font(_font), style(_style)
{
  unit_font.Load(FontDescription(std::max(font.GetHeight() * 3 / 5, 7u)));

  /* bold, its capitals filling two thirds of a battery as high as a
     capital letter, like on a phone */
  battery_font.Load(FontDescription(std::max(font.GetCapitalHeight(), 6u),
                                    true));
  value_types.fill(InfoBoxFactory::NUM_TYPES);
  item_rects.fill(PixelRect{0, 0, 0, 0});
  item_list.Clear();
  item_widths.fill(0);
}

StatusBarWindow::~StatusBarWindow() noexcept = default;

unsigned
StatusBarWindow::GetHeight(const Font &font) noexcept
{
  return font.GetHeight() + 2 * (Layout::GetTextPadding() / 2);
}

void
StatusBarWindow::Create(ContainerWindow &parent, PixelRect rc) noexcept
{
  PaintWindow::Create(parent, rc);

  if (style == UISettings::StatusBarStyle::TRANSPARENT ||
      style == UISettings::StatusBarStyle::TRANSLUCENT)
    SetTransparent();
}

void
StatusBarWindow::SetItemInsets(int left, int right) noexcept
{
  if (left == inset_left && right == inset_right)
    return;

  inset_left = left;
  inset_right = right;
  Invalidate();
}

int
StatusBarWindow::GetItemAt(PixelPoint p) const noexcept
{
  for (unsigned i = 0; i < item_list.n_items; ++i)
    if (item_rects[i].Contains(p))
      return int(i);
  return -1;
}

void
StatusBarWindow::OnItemClicked(int i) noexcept
{
  using Symbol = StatusBarItem::Symbol;

  if (i < 0 || unsigned(i) >= item_list.n_items) {
    /* next to the items */
    InputEvents::eventStatus("system");
    return;
  }

  const StatusBarItem item = item_list.items[i];
  if (item.kind != StatusBarItem::Kind::SYMBOL) {
    /* what a tap on the InfoBox opens, e.g. the times for a clock */
    if (value_contents[i] != nullptr)
      value_contents[i]->HandleClick();
    return;
  }

  switch (Symbol(item.id)) {
  case Symbol::BATTERY:
    InputEvents::eventStatus("system");
    break;

  case Symbol::GPS:
    HandleInternalLink("xcsoar://config/devices");
    break;

  case Symbol::FLARM:
    InputEvents::eventTraffic("show");
    break;

  case Symbol::BALLAST:
    HandleInternalLink("xcsoar://dialog/flight");
    break;

  case Symbol::LOGGER:
    HandleInternalLink("xcsoar://config/logger");
    break;

  case Symbol::NETWORK:
    if (!HandleInternalLink("xcsoar://config/network"))
      InputEvents::eventStatus("system");
    break;

  case Symbol::PAGE_NUMBER:
    PageActions::Next();
    break;

  case Symbol::PAGE_TITLE:
    HandleInternalLink("xcsoar://config/pages");
    break;

  case Symbol::DATA_SOURCE:
    if (CommonInterface::Basic().gps.replay)
      HandleInternalLink("xcsoar://dialog/replay");
    else
      InputEvents::eventStatus("system");
    break;

  case Symbol::COUNT:
    break;
  }
}

bool
StatusBarWindow::OnMouseDown(PixelPoint p) noexcept
{
  pressed = true;
  pressed_item = GetItemAt(p);
  SetCapture();
  return true;
}

bool
StatusBarWindow::OnMouseUp(PixelPoint p) noexcept
{
  ReleaseCapture();

  /* the action on lift-off; sliding off the item (or the bar)
     cancels */
  const bool clicked = pressed && GetClientRect().Contains(p) &&
    GetItemAt(p) == pressed_item;
  pressed = false;

  if (clicked)
    OnItemClicked(pressed_item);
  return true;
}

/**
 * Pick a colour for a state: the text colour when all is well, and on
 * e-paper, otherwise red or amber.
 */
[[gnu::const]]
static Color
StateColor(Color text_color, bool alarm, bool warning) noexcept
{
  if (!HasColors())
    return text_color;
  if (alarm)
    return COLOR_RED;
  if (warning)
    return Color(0xb3, 0x5a, 0x00);
  return text_color;
}

/**
 * The location arrow, with slightly rounded corners: filled with the
 * colour of the state, or on e-paper, outlined and filled only with
 * a fix.
 */
static void
DrawGPSIcon(Canvas &canvas, const PixelRect &box, Color color,
            bool outlined, bool filled) noexcept
{
  const double s = box.GetHeight();

  /* the tip at the top right, the wings, and a shallow notch at the
     back */
  struct Point { double x, y; };
  static constexpr Point shape[] = {
    {1, 0}, {0, 0.45}, {0.42, 0.58}, {0.55, 1},
  };
  constexpr unsigned n = std::size(shape);
  constexpr unsigned STEPS = 3;

  /* replace each outer corner by a curve which starts and ends this
     far away from it; the notch stays sharp */
  const double radius = 0.15;
  constexpr unsigned NOTCH = 2;

  BulkPixelPoint points[n * (STEPS + 1)];
  unsigned n_points = 0;
  for (unsigned i = 0; i < n; ++i) {
    const Point &p = shape[i];
    if (i == NOTCH) {
      points[n_points++] = {box.left + int(std::lround(p.x * s)),
                            box.top + int(std::lround(p.y * s))};
      continue;
    }

    const Point &a = shape[(i + n - 1) % n];
    const Point &b = shape[(i + 1) % n];

    const auto toward = [&p, radius](const Point &q){
      const double dx = q.x - p.x, dy = q.y - p.y;
      const double d = std::min(radius / std::hypot(dx, dy), 0.5);
      return Point{p.x + dx * d, p.y + dy * d};
    };
    const Point from = toward(a), to = toward(b);

    /* a quadratic Bézier curve around the corner */
    for (unsigned j = 0; j <= STEPS; ++j) {
      const double t = double(j) / STEPS, u = 1 - t;
      const double x = u * u * from.x + 2 * u * t * p.x + t * t * to.x;
      const double y = u * u * from.y + 2 * u * t * p.y + t * t * to.y;
      points[n_points++] = {box.left + int(std::lround(x * s)),
                            box.top + int(std::lround(y * s))};
    }
  }

  if (outlined)
    canvas.Select(Pen(Layout::ScalePenWidth(1), color));
  else
    canvas.SelectNullPen();
  if (filled)
    canvas.Select(Brush(color));
  else
    canvas.SelectHollowBrush();
  canvas.DrawPolygon(points, n_points);
}

/**
 * A glider seen from above.
 */
static void
DrawFlarmIcon(Canvas &canvas, PixelRect box, Color color) noexcept
{
  const int s = box.GetHeight();
  const int cx = box.GetCenter().x, y = box.top;
  const int wing_y = y + s * 7 / 20;

  canvas.Select(Pen(std::max(2u, unsigned(s) / 6), color));
  canvas.DrawLine({cx, y}, {cx, y + s});
  canvas.DrawLine({box.left, wing_y}, {box.right, wing_y});
  canvas.DrawLine({cx - s / 3, y + s}, {cx + s / 3, y + s});
}

/**
 * A cloud for the internet connection, grey without one.
 */
static void
DrawNetworkIcon(Canvas &canvas, PixelRect box, Color color) noexcept
{
  const int w = box.GetWidth(), h = box.GetHeight();
  const int x = box.left, y = box.top;

  canvas.SelectNullPen();
  canvas.Select(Brush(color));

  /* three puffs on a flat bottom */
  canvas.DrawCircle({x + w * 30 / 100, y + h * 65 / 100},
                    unsigned(h * 30 / 100));
  canvas.DrawCircle({x + w * 55 / 100, y + h * 45 / 100},
                    unsigned(h * 40 / 100));
  canvas.DrawCircle({x + w * 76 / 100, y + h * 66 / 100},
                    unsigned(h * 29 / 100));
  canvas.DrawFilledRectangle({x + w * 30 / 100, y + h * 60 / 100,
                              x + w * 76 / 100, y + h * 95 / 100},
                             color);
}

/**
 * A battery like on a phone: no outline, the charge left filled in
 * the given colour, the rest grey, and the charge written inside.
 */
static void
DrawBatteryIcon(Canvas &canvas, const PixelRect &box, const Font &font,
                Color level_color, Color empty_color, Color number_color,
                unsigned percent) noexcept
{
  const int height = box.GetHeight();
  const int nub_width = std::max(2, height / 6);
  const PixelRect body{box.left, box.top, box.right - nub_width - 1,
                       box.bottom};
  /* round corners, but no pill */
  const PixelSize corner(unsigned(height) * 2 / 5);
  const int round = height / 5;

  canvas.SelectNullPen();
  canvas.Select(Brush(empty_color));
  canvas.DrawRoundRectangle(body, corner);

  /* the charge left, clipped to the rounded body */
  PixelRect level = body;
  level.right = level.left + level.GetWidth() * int(std::min(percent, 100u))
    / 100;
  if (level.right > level.left) {
    canvas.Select(Brush(level_color));
    if (level.right >= body.right - round)
      canvas.DrawRoundRectangle(level, corner);
    else {
      /* round on the left, straight on the right */
      canvas.DrawRoundRectangle({level.left, level.top,
                                 std::min(level.right + round, body.right),
                                 level.bottom}, corner);
      canvas.DrawFilledRectangle({level.right, level.top,
                                  std::min(level.right + round, body.right),
                                  level.bottom}, empty_color);
    }
  }

  canvas.DrawFilledRectangle({{body.right + 1, box.top + height / 3},
                              PixelSize{nub_width,
                                        height - 2 * (height / 3)}},
                             empty_color);

  /* the number in the middle, its capitals centred */
  StaticString<8> text;
  text.Format("%u", percent);
  canvas.Select(font);
  canvas.SetTextColor(number_color);
  const int width = canvas.CalcTextSize(text).width;
  const PixelPoint p{body.GetCenter().x - width / 2,
                     body.GetCenter().y - int(font.GetAscentHeight()) +
                     int(font.GetCapitalHeight()) / 2};
  canvas.DrawText(p, text);
  /* once more, a pixel to the right: heavier, like on a phone */
  canvas.DrawText(p.At(1, 0), text);
}

/**
 * A drop of water.
 */
static void
DrawBallastIcon(Canvas &canvas, PixelRect box, Color color) noexcept
{
  const int s = box.GetHeight();
  const int cx = box.GetCenter().x;
  const int radius = s * 3 / 10;
  const PixelPoint center{cx, box.bottom - radius};
  const BulkPixelPoint tip[] = {
    {cx, box.top},
    {cx - radius, center.y},
    {cx + radius, center.y},
  };

  canvas.SelectNullPen();
  canvas.Select(Brush(color));
  canvas.DrawPolygon(tip, std::size(tip));
  canvas.DrawCircle(center, radius);
}

void
StatusBarWindow::OnPaint(Canvas &canvas) noexcept
{
  const PixelRect bar_rc = GetClientRect();

  /* the items, clear of the display cutout and the rounded corners */
  const PixelRect rc{bar_rc.left + inset_left, bar_rc.top,
                     bar_rc.right - inset_right, bar_rc.bottom};

  const bool dark = style == UISettings::StatusBarStyle::BLACK;
  const Color text_color = dark ? COLOR_WHITE : COLOR_BLACK;
  const Color background_color = dark ? COLOR_BLACK : COLOR_WHITE;

  switch (style) {
  case UISettings::StatusBarStyle::WHITE:
    canvas.Clear(COLOR_WHITE);
    /* a hairline sets the bar off the screen below; not with
       DrawHLine(), whose width is whatever the last pen left behind
       on OpenGL */
    canvas.DrawFilledRectangle({bar_rc.left, bar_rc.bottom - 1,
                                bar_rc.right, bar_rc.bottom},
                               COLOR_LIGHT_GRAY);
    break;

  case UISettings::StatusBarStyle::BLACK:
    canvas.Clear(COLOR_BLACK);
    break;

  case UISettings::StatusBarStyle::TRANSPARENT:
    /* the map below shows through */
    break;

  case UISettings::StatusBarStyle::TRANSLUCENT: {
    /* the map below shows through a white veil, like through a
       rounded map label (see TextInBox()) */
#ifdef ENABLE_OPENGL
    const ScopeAlphaBlend alpha_blend;
    canvas.DrawFilledRectangle(bar_rc, COLOR_WHITE.WithAlpha(0xa0));
#else
    canvas.Clear(COLOR_WHITE);
#endif
    break;
  }
  }

  canvas.Select(font);
  canvas.SetBackgroundTransparent();

  const int gap = Layout::Scale(6);
  /* the capital letters in the middle, which looks centred, unlike
     the whole line with its descent; in the place of the system
     status bar (taller than the bar itself), a little below the
     middle, where iOS puts its own items (measured on an iPhone: 52.5
     percent of the height) */
  const int height = rc.GetHeight();
  const int middle = height > int(GetHeight(font))
    ? height * 21 / 40
    : height / 2;
  const int text_top = middle + int(font.GetCapitalHeight()) / 2
    - int(font.GetAscentHeight());

  const auto &settings_items =
    CommonInterface::GetUISettings().status_bar_items;
  if (!(settings_items == item_list)) {
    /* other items: forget the widths of the old ones */
    item_list = settings_items;
    item_widths.fill(0);
  }

  using Zone = StatusBarItem::Zone;
  const auto items = item_list.GetItems();
  const unsigned n_left = item_list.GetZoneBegin(Zone::RIGHT);

  /* the items drawn below tell where they are, for the taps */
  item_rects.fill(PixelRect{0, 0, 0, 0});
  const unsigned first_right = n_left;

  /* left: from the left edge up to the middle; where they run out of
     room, the inner ones are left out */
  /* InfoBox values next to each other sit closer together */
  const auto gap_after = [&](unsigned a, unsigned b){
    return items[a].kind == StatusBarItem::Kind::INFOBOX &&
      items[b].kind == StatusBarItem::Kind::INFOBOX
      ? Layout::Scale(3)
      : gap;
  };

  /* lay out the items on the left, from the left edge up to the
     middle; where they run out of room, the inner ones are left out */
  const auto layout_left = [&](bool draw){
    measuring = !draw;
    int x = rc.left + gap;
    for (unsigned i = 0; i < n_left; ++i) {
      const int width = DrawItem(canvas, i, items[i],
                                 {x, rc.top, rc.GetCenter().x, rc.bottom},
                                 true, text_top, text_color,
                                 background_color);
      if (width < 0)
        break;
      if (width > 0)
        x += width + (i + 1 < n_left ? gap_after(i, i + 1) : gap);
    }
    measuring = false;
    return x;
  };

  /* the width all items on the right would take */
  const auto measure_right = [&](){
    measuring = true;
    int total = 0;
    for (unsigned i = items.size(); i-- > first_right;) {
      const int width = DrawItem(canvas, i, items[i],
                                 {0, rc.top, 1 << 20, rc.bottom},
                                 false, text_top, text_color,
                                 background_color);
      if (width > 0)
        total += width + (i > first_right ? gap_after(i - 1, i) : gap);
    }
    measuring = false;
    return total;
  };

  /* make room for all items on the right before leaving any out:
     abbreviate the data source ("SIM") */
  source_abbreviated = false;
  source_abbreviated =
    measure_right() > rc.right - gap - layout_left(false);

  const int left = layout_left(true);

  /* right: from the right edge up to the items on the left */
  int right = rc.right - gap;
  for (unsigned i = items.size(); i-- > first_right;) {
    const int width = DrawItem(canvas, i, items[i],
                               {left, rc.top, right, rc.bottom},
                               false, text_top, text_color,
                               background_color);
    if (width < 0)
      break;
    if (width > 0)
      right -= width + (i > first_right ? gap_after(i - 1, i) : gap);
  }
}

int
StatusBarWindow::DrawPageNumber(Canvas &canvas, unsigned i,
                                const PixelRect &zone,
                                bool from_left, Color text_color,
                                Color background_color) noexcept
{
  /* the position of the page in a small badge, e.g. "2/5" */
  const auto &ui_state = CommonInterface::GetUIState();
  StaticString<16> position;
  position.Format("%u/%u", ui_state.map_page_number,
                  ui_state.map_page_count);

  canvas.Select(unit_font);
  const int padding = Layout::GetTextPadding() / 2;
  const int side_padding = Layout::GetTextPadding();
  const int width = canvas.CalcTextSize(position).width + 2 * side_padding;
  if (width > zone.right - zone.left || measuring) {
    canvas.Select(font);
    return width > zone.right - zone.left ? -1 : width;
  }

  const int x = from_left ? zone.left : zone.right - width;
  SetItemRect(i, {x, zone.top, x + width, zone.bottom});
  const int height = unit_font.GetHeight() + padding;
  const int top = zone.GetCenter().y - height / 2;
  canvas.SelectNullPen();
  canvas.Select(Brush(text_color));
  canvas.DrawRoundRectangle({{x, top}, PixelSize{width, height}},
                            PixelSize(unsigned(height) / 2));

  /* in the middle of the badge */
  canvas.SetTextColor(background_color);
  canvas.DrawText({x + side_padding,
                   top + (height - int(unit_font.GetHeight())) / 2},
                  position);
  canvas.Select(font);
  return width;
}

int
StatusBarWindow::DrawItem(Canvas &canvas, unsigned i, StatusBarItem item,
                          const PixelRect &zone, bool from_left,
                          int text_top, Color text_color,
                          Color background_color) noexcept
{
  /* an item is a symbol and a text left or right of it, each
     optional */
  const int padding = Layout::GetTextPadding();
  const int icon_size = font.GetCapitalHeight();
  const int icon_top = text_top + (int)font.GetAscentHeight() - icon_size;

  StaticString<48> text;
  text.clear();

  /* the unit of an InfoBox value, in a smaller font */
  const char *unit = nullptr;
  Color color = text_color;
  int icon_width = 0;
  bool icon_left = true;

  const auto &basic = CommonInterface::Basic();

  enum class Icon {
    NONE, BATTERY, GPS, FLARM, BALLAST, LOGGER, NETWORK,
  } icon = Icon::NONE;

  unsigned battery_percent = 0;
  bool charging = false;

  switch (item.kind) {
  case StatusBarItem::Kind::SYMBOL:
    switch (StatusBarItem::Symbol(item.id)) {
    case StatusBarItem::Symbol::BATTERY: {
      bool have_battery = DEBUG_ALL_MAP_OVERLAYS;
      battery_percent = 57;
#ifdef HAVE_BATTERY
      if (const auto &battery = Power::global_info.battery;
          battery.remaining_percent) {
        have_battery = true;
        battery_percent = *battery.remaining_percent;
        charging = Power::global_info.external.status ==
          Power::ExternalInfo::Status::ON;
      }
#endif
      if (have_battery) {
        color = StateColor(text_color, battery_percent < 10,
                           battery_percent < 25);
        icon = Icon::BATTERY;
        icon_left = false;
        /* the charge inside the battery, like on a phone */
        canvas.Select(battery_font);
        icon_width = canvas.CalcTextSize("100").width + 1 +
          2 * Layout::Scale(2) + std::max(2, icon_size / 6) + 1;
        canvas.Select(font);
      }
      break;
    }

    case StatusBarItem::Symbol::GPS:
      /* red without a GPS, amber while waiting for a fix, like the
         FLARM alarms; yellow would get lost on a light bar; grey
         without a GPS on e-paper */
      color = HasColors()
        ? StateColor(text_color, !basic.alive, !basic.location_available)
        : (basic.alive ? text_color : COLOR_GRAY);
      icon = Icon::GPS;
      icon_width = icon_size;
      break;

    case StatusBarItem::Symbol::FLARM:
      if (const auto &flarm = basic.flarm.status;
          flarm.available || DEBUG_ALL_MAP_OVERLAYS) {
        using AlarmType = FlarmTraffic::AlarmType;
        color = StateColor(text_color,
                           flarm.alarm_level == AlarmType::IMPORTANT ||
                           flarm.alarm_level == AlarmType::URGENT,
                           flarm.alarm_level == AlarmType::LOW ||
                           flarm.alarm_level == AlarmType::INFO_ALERT);
        if (const unsigned n = DEBUG_ALL_MAP_OVERLAYS
              ? 3
              : basic.flarm.traffic.GetActiveTrafficCount();
            n > 0)
          text.Format("%u", n);
        icon = Icon::FLARM;
        icon_width = icon_size * 3 / 2;
        icon_left = false;
      }
      break;

    case StatusBarItem::Symbol::BALLAST:
      if (const auto &polar = CommonInterface::GetComputerSettings().polar;
          polar.ballast_timer_active || DEBUG_ALL_MAP_OVERLAYS) {
        text.Format("%.0f l", DEBUG_ALL_MAP_OVERLAYS
                    ? 120.
                    : polar.glide_polar_task.GetBallastLitres());
        icon = Icon::BALLAST;
        icon_width = icon_size * 3 / 4;
      }
      break;

    case StatusBarItem::Symbol::LOGGER:
      if (DEBUG_ALL_MAP_OVERLAYS ||
          (backend_components != nullptr &&
           backend_components->igc_logger != nullptr &&
           backend_components->igc_logger->IsLoggerActive())) {
        icon = Icon::LOGGER;
        icon_width = icon_size * 3 / 4;
      }
      break;

    case StatusBarItem::Symbol::NETWORK:
      if (const NetState state = DEBUG_ALL_MAP_OVERLAYS
            ? NetState::DISCONNECTED
            : GetNetState();
          state != NetState::UNKNOWN) {
        color = state != NetState::DISCONNECTED
          ? text_color
          : COLOR_GRAY;
        icon = Icon::NETWORK;
        icon_width = icon_size * 3 / 2;
      }
      break;

    case StatusBarItem::Symbol::PAGE_NUMBER:
      if (const auto &ui_state = CommonInterface::GetUIState();
          ui_state.map_page_number > 0)
        return DrawPageNumber(canvas, i, zone, from_left, text_color,
                              background_color);
      break;

    case StatusBarItem::Symbol::PAGE_TITLE:
      /* a plain map page has no short title, but there is room to
         name it here */
      text = CommonInterface::GetUIState().map_page_title.c_str();
      if (*text == '\0')
        text = _("Map");
      break;

    case StatusBarItem::Symbol::DATA_SOURCE: {
      /* abbreviated where the full text does not fit */
      StaticString<32> source;
      FormatDataSource(source, basic, false);
      if (!source.empty() &&
          (source_abbreviated ||
           (int)canvas.CalcTextSize(source).width > zone.right - zone.left))
        FormatDataSource(source, basic, true);
      text = source.c_str();
      break;
    }

    case StatusBarItem::Symbol::COUNT:
      break;
    }
    break;

  case StatusBarItem::Kind::INFOBOX:
    if (value_types[i] != item.id) {
      value_types[i] = item.id;
      value_contents[i] =
        InfoBoxFactory::Create(InfoBoxFactory::Type(item.id));
    }

    if (value_contents[i] != nullptr) {
      InfoBoxData data;
      data.Clear();
      value_contents[i]->Update(data);
      /* a graphical InfoBox has no value to show */
      if (!data.value.empty()) {
        text = data.value.c_str();
        if (data.value_unit != Unit::UNDEFINED)
          unit = Units::GetUnitName(data.value_unit);
      }
    }
    break;
  }

  const int value_width =
    text.empty() ? 0 : canvas.CalcTextSize(text).width;
  int unit_width = 0;
  if (unit != nullptr && *unit != '\0') {
    canvas.Select(unit_font);
    unit_width = padding / 2 + canvas.CalcTextSize(unit).width;
    canvas.Select(font);
  }

  /* keep the width of a number while its digits change, lest the
     whole bar wobbles: measure the text with zeroes for digits, and
     never shrink; a text like the page title takes its own width */
  const bool is_number = item.kind == StatusBarItem::Kind::INFOBOX ||
    icon == Icon::BATTERY || icon == Icon::FLARM || icon == Icon::BALLAST;
  int text_width = value_width + unit_width;
  if (value_width > 0 && is_number) {
    StaticString<48> sample(text);
    for (char *p = sample.buffer(); *p != '\0'; ++p)
      if (*p >= '1' && *p <= '9')
        *p = '0';
    text_width = std::max({text_width,
                           (int)canvas.CalcTextSize(sample).width +
                           unit_width,
                           item_widths[i]});
    item_widths[i] = text_width;
  }

  int width = text_width + icon_width;
  if (text_width > 0 && icon_width > 0)
    width += padding;
  if (width == 0)
    /* nothing to say */
    return 0;

  if (width > zone.right - zone.left)
    return -1;

  if (measuring)
    return width;

  const int right = from_left ? zone.left + width : zone.right;
  SetItemRect(i, {right - width, zone.top, right, zone.bottom});

  int icon_x, text_x;
  if (icon_left) {
    icon_x = right - width;
    text_x = right - text_width;
  } else {
    text_x = right - width;
    icon_x = right - icon_width;
  }

  /* right-aligned in the width reserved for the text */
  text_x += text_width - value_width - unit_width;

  if (value_width > 0) {
    canvas.SetTextColor(color);
    canvas.DrawText({text_x, text_top}, text);
  }

  if (unit_width > 0) {
    /* on the same baseline as the value */
    canvas.Select(unit_font);
    canvas.DrawText({text_x + value_width + padding / 2,
                     text_top + int(font.GetAscentHeight()) -
                     int(unit_font.GetAscentHeight())},
                    unit);
    canvas.Select(font);
  }

  const PixelRect box{{icon_x, icon_top}, PixelSize{icon_width, icon_size}};
  switch (icon) {
  case Icon::NONE:
    break;

  case Icon::BATTERY:
    {
      const bool dark = style == UISettings::StatusBarStyle::BLACK;
      Color level_color = text_color;
      if (HasColors() && charging)
        level_color = Color(0x34, 0xc7, 0x59);
      else if (HasColors() && battery_percent < 20)
        level_color = COLOR_RED;
      const Color empty_color = dark
        ? Color(0x5a, 0x5a, 0x5a)
        : Color(0xc4, 0xc4, 0xc4);
      DrawBatteryIcon(canvas, box, battery_font, level_color, empty_color,
                      background_color, battery_percent);
    }
    canvas.Select(font);
    break;

  case Icon::GPS:
    if (!HasColors())
      /* e-paper: filled with a fix, only outlined without */
      DrawGPSIcon(canvas, box, color, true, basic.location_available);
    else
      /* just in the colour of the state */
      DrawGPSIcon(canvas, box, color, false, true);
    break;

  case Icon::FLARM:
    DrawFlarmIcon(canvas, box, color);
    break;

  case Icon::BALLAST:
    DrawBallastIcon(canvas, box, color);
    break;

  case Icon::LOGGER:
    /* a red dot, like the recording light of a camera */
    canvas.SelectNullPen();
    canvas.Select(Brush(StateColor(text_color, true, false)));
    canvas.DrawCircle(box.GetCenter(), unsigned(box.GetWidth()) / 2);
    break;

  case Icon::NETWORK:
    DrawNetworkIcon(canvas, box, color);
    break;
  }

  return width;
}
