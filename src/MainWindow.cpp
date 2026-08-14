// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project
#include "MainWindow.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "PopupMessage.hpp"
#include "InfoBoxes/InfoBoxManager.hpp"
#include "InfoBoxes/InfoBoxLayout.hpp"
#include "UIActions.hpp"
#include "PageActions.hpp"
#include "Input/InputEvents.hpp"
#include "Menu/MenuBar.hpp"
#include "Menu/Glue.hpp"
#include "ui/canvas/Features.hpp" // for DRAW_MOUSE_CURSOR
#include "Screen/Layout.hpp"
#include "Dialogs/Airspace/AirspaceWarningDialog.hpp"
#include "Audio/Sound.hpp"
#include "ProcessTimer.hpp"
#include "LogFile.hpp"
#include "Gauge/GaugeFLARM.hpp"
#include "Gauge/GaugeThermalAssistant.hpp"
#include "Gauge/GlueGaugeVario.hpp"
#include "Form/Form.hpp"
#include "Widget/Widget.hpp"
#include "Look/GlobalFonts.hpp"
#include "Look/DefaultFonts.hpp"
#include "Look/Look.hpp"
#include "Operation/PopupOperationEnvironment.hpp"
#include "Operation/PluggableOperationEnvironment.hpp"
#include "Device/MultipleDevices.hpp"
#include "ProgressGlue.hpp"
#include "UIState.hpp"
#include "DrawThread.hpp"
#include "UIReceiveBlackboard.hpp"
#include "UISettings.hpp"
#include "Interface.hpp"

#include <utility>
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "Storage/StorageManager.hpp"
#include "Storage/StorageEvents.hpp"

#ifdef USE_WINUSER
#include "Storage/win/WinHotplugForward.hpp"
#endif

#ifdef ANDROID
#include "Android/ReceiveTask.hpp"
#include "Android/Main.hpp"
#include "Android/NativeView.hpp"
#include "Engine/Task/Ordered/OrderedTask.hpp"
#include "Dialogs/Task/TaskDialogs.hpp"
#include "ui/event/Globals.hpp"
#include "ui/event/Queue.hpp"
#include "java/Global.hxx"
#endif

static constexpr unsigned separator_height = 2;

/**
 * The OVERLAY border style packs title, value and comment tightly
 * (see InfoBoxWindow::OnResize()), so its boxes get by with less
 * height than the classic styles.
 */
static constexpr unsigned overlay_row_height_percent = 75;

[[gnu::pure]]
static bool
IsOverlayBorderStyle() noexcept
{
  return CommonInterface::GetUISettings().info_boxes.border_style ==
    InfoBoxSettings::BorderStyle::OVERLAY;
}

[[gnu::pure]]
static unsigned
GetInfoBoxRowHeightPercent() noexcept
{
  return IsOverlayBorderStyle() ? overlay_row_height_percent : 100;
}

[[gnu::pure]]
static PixelRect
GetMapOverlayButtonRect(const PixelRect rc, int top) noexcept
{
  const unsigned padding = Layout::GetTextPadding();
  const unsigned size = std::max(1u, Layout::GetMaximumControlHeight());

  if (rc.top >= rc.bottom || rc.left >= rc.right)
    return PixelRect(rc.left, rc.top, rc.left + int(size), rc.top + int(size));

  int bottom = top + int(size);
  if (bottom > rc.bottom)
    top = rc.bottom - int(size);
  if (top < rc.top)
    top = rc.top;

  int right = rc.right - int(padding);
  int left = right - int(size);
  if (left < rc.left) {
    left = rc.left;
    right = left + int(size);
  }
  if (right > rc.right)
    right = rc.right;

  bottom = top + int(size);
  if (bottom <= top)
    bottom = top + int(size);

  return PixelRect(left, top, right, bottom);
}

[[gnu::pure]]
PixelRect
MainWindow::GetShowMenuButtonRect(const PixelRect rc) noexcept
{
  return GetMapOverlayButtonRect(rc, rc.top + Layout::GetTextPadding());
}

/**
 * The width of the overlay button column in the top right corner, or 0
 * if there is none.
 */
[[gnu::pure]]
static unsigned
GetMapOverlayTopRightWidth(const PixelRect rc) noexcept
{
  const UISettings &settings = CommonInterface::GetUISettings();

  if (!settings.show_menu_button && !settings.show_quickmenu_button &&
      !settings.show_zoom_button)
    return 0;

  const PixelRect button_rc = GetMapOverlayButtonRect(rc, rc.top);
  return unsigned(std::max(0, rc.right - button_rc.left));
}

[[gnu::pure]]
PixelRect
MainWindow::GetShowQuickMenuButtonRect(const PixelRect rc) noexcept
{
  const UISettings &settings = CommonInterface::GetUISettings();
  const unsigned padding = Layout::GetTextPadding();

  int top = rc.top + int(padding);
  if (settings.show_menu_button)
    top = GetShowMenuButtonRect(rc).bottom + int(padding);

  return GetMapOverlayButtonRect(rc, top);
}

[[gnu::pure]]
PixelRect
MainWindow::GetShowZoomButtonRect(const PixelRect rc,
                                  ShowZoomButton::Sign sign) noexcept
{
  const UISettings &settings = CommonInterface::GetUISettings();
  const unsigned padding = Layout::GetTextPadding();

  int top = rc.top + int(padding);
  if (settings.show_quickmenu_button)
    top = GetShowQuickMenuButtonRect(rc).bottom + int(padding);
  else if (settings.show_menu_button)
    top = GetShowMenuButtonRect(rc).bottom + int(padding);

  if (sign == ShowZoomButton::Sign::ZOOM_OUT) {
    const PixelRect zoom_in =
      GetShowZoomButtonRect(rc, ShowZoomButton::Sign::ZOOM_IN);
    top = zoom_in.bottom + int(padding);
  }

  return GetMapOverlayButtonRect(rc, top);
}

#ifdef ANDROID
[[gnu::pure]]
PixelRect
MainWindow::GetShowRotateButtonRect(const PixelRect rc) noexcept
{
  const unsigned padding = Layout::GetTextPadding();
  const unsigned size = Layout::GetMaximumControlHeight();
  const int left = rc.left + padding;
  const int right = left + size;
  const int top = rc.top + padding;
  const int bottom = top + size;

  return PixelRect(left, top, right, bottom);
}
#endif

[[gnu::pure]]
static PixelRect
GetTopWidgetRect(const PixelRect &rc, const Widget *top_widget) noexcept
{
  if (top_widget == nullptr) {
    /* no top widget: return empty rectangle, map uses the whole main
       area */
    PixelRect result = rc;
    result.bottom = result.top;
    return result;
  }

  const unsigned requested_height = top_widget->GetMinimumSize().height;
  unsigned height;
  if (requested_height > 0) {
    const unsigned max_height = rc.GetHeight() / 2;
    height = std::min(max_height, requested_height);
  } else {
    const unsigned recommended_height = rc.GetHeight() / 4;
    height = recommended_height;
  }

  PixelRect result = rc;
  result.bottom = result.top + height;
  return result;
}

[[gnu::pure]]
static PixelRect
GetBottomWidgetRect(const PixelRect &rc, const Widget *bottom_widget) noexcept
{
  if (bottom_widget == nullptr) {
    /* no bottom widget: return empty rectangle, map uses the whole
       main area */
    PixelRect result = rc;
    result.top = result.bottom;
    return result;
  }

  const unsigned requested_height = bottom_widget->GetMinimumSize().height;
  unsigned height;
  if (requested_height > 0) {
    const unsigned max_height = rc.GetHeight() / 2;
    height = std::min(max_height, requested_height);
  } else {
    const unsigned recommended_height = rc.GetHeight() / 3;
    height = recommended_height;
  }

  PixelRect result = rc;
  result.top = result.bottom - height;
  return result;
}

[[gnu::pure]]
static PixelRect
GetMapRectAbove(const PixelRect &rc, const PixelRect &bottom_rect) noexcept
{
  PixelRect result = rc;
  result.bottom = bottom_rect.top;
  if (bottom_rect.top < bottom_rect.bottom)
    result.bottom -= separator_height;
  return result;
}

[[gnu::pure]]
static PixelRect
GetMapRectBelow(const PixelRect &rc, const PixelRect &top_rect) noexcept
{
  PixelRect result = rc;
  result.top = top_rect.bottom;
  if (top_rect.top < top_rect.bottom)
    result.top += separator_height;
  return result;
}

[[gnu::pure]]
static PixelRect
ComputeMapAreaRect(const PixelRect &main_rect,
                   const Widget *top_widget,
                   const Widget *bottom_widget) noexcept
{
  PixelRect rc = main_rect;

  const PixelRect top_rect = GetTopWidgetRect(rc, top_widget);
  rc = GetMapRectBelow(rc, top_rect);

  const PixelRect bottom_rect = GetBottomWidgetRect(rc, bottom_widget);
  return GetMapRectAbove(rc, bottom_rect);
}

PixelRect
MainWindow::GetMapDisplayRect(PixelRect rc) const noexcept
{
  if (!IsOverlayBorderStyle())
    /* only the overlay theme draws the map behind the InfoBoxes, and
       only there does it make sense to draw it under the notch */
    return rc;

  /* extend to the display edges wherever the rect already reaches the
     safe area, so the map fills the screen while InfoBoxes and HUD
     elements stay inside the safe area */
  const PixelRect safe_rc = GetClientRect();
  const PixelRect display_rc = GetDisplayRect();

  if (rc.left <= safe_rc.left)
    rc.left = display_rc.left;
  if (rc.top <= safe_rc.top)
    rc.top = display_rc.top;
  if (rc.right >= safe_rc.right)
    rc.right = display_rc.right;
  if (rc.bottom >= safe_rc.bottom)
    rc.bottom = display_rc.bottom;

  return rc;
}

PixelRect
MainWindow::GetMapContentRect() const noexcept
{
  if (map == nullptr ||
      CommonInterface::GetUISettings().info_boxes.border_style !=
      InfoBoxSettings::BorderStyle::OVERLAY)
    /* other themes do not extend the map beyond the safe area; let
       #GlueMapWindow use its own client rect */
    return PixelRect{0, 0, 0, 0};

  PixelRect rc;
  if (FullScreen) {
    /* no InfoBoxes are shown, so HUD elements may use the whole safe
       area - but not the display margins covered by the notch */
    rc = GetClientRect();
  } else {
    /* shrink by the visual box gap so overlays and aircraft centering
       are relative to the inner edge of the rounded boxes, not the
       InfoBox window edges */
    rc = overlay_rect;
    rc.Grow(-Layout::Scale(4));
  }

  /* #GlueMapWindow draws in its own client coordinates starting at
     (0,0), but the rects above are in MainWindow coordinates */
  const PixelRect map_position = map->GetPosition();
  rc.Offset(-map_position.left, -map_position.top);
  return rc;
}

PixelRect
MainWindow::GetMapAreaRect() const noexcept
{
  /* overlay_rect is set as soon as the layout has been calculated,
     which happens before the map window is created; use it also for
     the overlay buttons created during InitialiseConfigured() */
  if (overlay_rect.GetWidth() > 0)
    return overlay_rect;

  return ComputeMapAreaRect(GetMainRect(), top_widget, bottom_widget);
}

PixelRect
MainWindow::GetOverlayFreeRect() const noexcept
{
  /* intersection of the main rect (excludes top/bottom widgets) and
     the non-InfoBox area, so overlay elements stay clear of both */
  PixelRect rc = GetMainRect();
  if (!FullScreen && overlay_rect.GetWidth() > 0) {
    rc.left = std::max(rc.left, overlay_rect.left);
    rc.top = std::max(rc.top, overlay_rect.top);
    rc.right = std::min(rc.right, overlay_rect.right);
    rc.bottom = std::min(rc.bottom, overlay_rect.bottom);
  }
  return rc;
}

PixelRect
MainWindow::GetWidgetAreaRect() const noexcept
{
  /* in the OVERLAY theme the InfoBoxes float above the map, so top and
     bottom widgets have to live in the non-InfoBox area - otherwise
     they end up behind the boxes */
  return IsOverlayBorderStyle() ? GetOverlayFreeRect() : GetMainRect();
}

void
MainWindow::BeginCoalesceMapLayout() noexcept
{
  if (coalesce_map_layout++ != 0)
    return;

  coalesce_map_redraw = map != nullptr;
  if (coalesce_map_redraw)
    map->BeginCoalesceFullRedraw();
}

void
MainWindow::EndCoalesceMapLayout() noexcept
{
  assert(coalesce_map_layout > 0);

  if (--coalesce_map_layout > 0)
    return;

  if (map_layout_pending) {
    map_layout_pending = false;
    LayoutMapArea();
    UpdateMapOverlayButtonLayout();
  }

  if (coalesce_map_redraw) {
    coalesce_map_redraw = false;
    if (map != nullptr)
      map->EndCoalesceFullRedraw();
  }
}

void
MainWindow::LayoutMapArea() noexcept
{
  if (map == nullptr)
    return;

  if (coalesce_map_layout > 0) {
    map_layout_pending = true;
    return;
  }

  const bool overlay = IsOverlayBorderStyle();

  PixelRect main_rect = GetWidgetAreaRect();

  const PixelRect top_rect = GetTopWidgetRect(main_rect, top_widget);
  if (HaveTopWidget())
    top_widget->Move(top_rect);

  main_rect = GetMapRectBelow(main_rect, top_rect);

  const PixelRect bottom_rect = GetBottomWidgetRect(main_rect, bottom_widget);
  if (HaveBottomWidget())
    bottom_widget->Move(bottom_rect);

  /* the overlay theme keeps the map at full size below everything
     else; other themes shrink it to the area left by the widgets */
  map->Move(overlay
            ? GetMapDisplayRect(GetMainRect())
            : GetMapRectAbove(main_rect, bottom_rect));
}

void
MainWindow::UpdateMapOverlayButtonLayout() noexcept
{
  const bool overlay_buttons_active =
    widget == nullptr && map != nullptr &&
    PageActions::AllowMapOverlayButtons();

  if (show_menu_button != nullptr) {
    show_menu_button->SetVisible(overlay_buttons_active);
    show_menu_button->SetEnabled(overlay_buttons_active);
    if (overlay_buttons_active)
      show_menu_button->Move(GetShowMenuButtonRect(GetMapAreaRect()));
  }
  if (show_quickmenu_button != nullptr) {
    show_quickmenu_button->SetVisible(overlay_buttons_active);
    show_quickmenu_button->SetEnabled(overlay_buttons_active);
    if (overlay_buttons_active)
      show_quickmenu_button->Move(GetShowQuickMenuButtonRect(GetMapAreaRect()));
  }
  if (show_zoom_out_button != nullptr) {
    show_zoom_out_button->SetVisible(overlay_buttons_active);
    show_zoom_out_button->SetEnabled(overlay_buttons_active);
    if (overlay_buttons_active)
      show_zoom_out_button->Move(GetShowZoomButtonRect(GetMapAreaRect(),
                                                       ShowZoomButton::Sign::ZOOM_OUT));
  }
  if (show_zoom_in_button != nullptr) {
    show_zoom_in_button->SetVisible(overlay_buttons_active);
    show_zoom_in_button->SetEnabled(overlay_buttons_active);
    if (overlay_buttons_active)
      show_zoom_in_button->Move(GetShowZoomButtonRect(GetMapAreaRect(),
                                                      ShowZoomButton::Sign::ZOOM_IN));
  }

#ifdef ANDROID
  if (show_rotate_button != nullptr && overlay_buttons_active)
    show_rotate_button->Move(GetShowRotateButtonRect(GetMapAreaRect()));
#endif

  if (map != nullptr)
    /* keep the north arrow clear of the overlay buttons */
    map->SetTopRightMargin(overlay_buttons_active
                           ? GetMapOverlayTopRightWidth(GetMapAreaRect())
                           : 0);

  /* Newly created overlay buttons are added after the map; keep the map
     underneath them (same as ReinitialiseLayout()). */
  if (overlay_buttons_active)
    map->BringToBottom();
}

void
MainWindow::ReinitialiseMapOverlayButtons() noexcept
{
  if (look == nullptr)
    return;

  const UISettings &settings = CommonInterface::GetUISettings();
  const PixelRect map_area_rect = GetMapAreaRect();

  if (settings.show_menu_button) {
    if (show_menu_button == nullptr) {
      show_menu_button = new ShowMenuButton();
      show_menu_button->Create(*this, look->dialog.button,
                               GetShowMenuButtonRect(map_area_rect));
    }
  } else if (show_menu_button != nullptr) {
    delete show_menu_button;
    show_menu_button = nullptr;
  }

  if (settings.show_quickmenu_button) {
    if (show_quickmenu_button == nullptr) {
      show_quickmenu_button = new ShowQuickMenuButton();
      show_quickmenu_button->Create(*this, look->dialog.button,
                                    GetShowQuickMenuButtonRect(map_area_rect));
    }
  } else if (show_quickmenu_button != nullptr) {
    delete show_quickmenu_button;
    show_quickmenu_button = nullptr;
  }

  if (settings.show_zoom_button) {
    if (show_zoom_out_button == nullptr) {
      show_zoom_out_button = new ShowZoomButton();
      show_zoom_out_button->Create(*this, look->dialog.button,
                                   GetShowZoomButtonRect(map_area_rect,
                                                         ShowZoomButton::Sign::ZOOM_OUT),
                                   ShowZoomButton::Sign::ZOOM_OUT);
    }
    if (show_zoom_in_button == nullptr) {
      show_zoom_in_button = new ShowZoomButton();
      show_zoom_in_button->Create(*this, look->dialog.button,
                                  GetShowZoomButtonRect(map_area_rect,
                                                        ShowZoomButton::Sign::ZOOM_IN),
                                  ShowZoomButton::Sign::ZOOM_IN);
    }
  } else {
    delete show_zoom_out_button;
    show_zoom_out_button = nullptr;
    delete show_zoom_in_button;
    show_zoom_in_button = nullptr;
  }

  UpdateMapOverlayButtonLayout();
}

MainWindow::MainWindow(UI::Display &display) noexcept
  : SingleWindow(display) {}

/**
 * Destructor of the MainWindow-Class
 * @return
 */
MainWindow::~MainWindow() noexcept
{
  Destroy();
}

void
MainWindow::Create(PixelSize size, UI::TopWindowStyle style)
{
  SingleWindow::Create(title, size, style);
}

void
MainWindow::Initialise()
{
  Layout::Initialise(GetDisplay(), GetSize(),
                     CommonInterface::GetUISettings().GetPercentScale(),
                     CommonInterface::GetUISettings().custom_dpi);
#ifdef DRAW_MOUSE_CURSOR
  SetCursorSize(CommonInterface::GetDisplaySettings().cursor_size);
  SetCursorColorsInverted(CommonInterface::GetDisplaySettings().invert_cursor_colors);
#endif

  Fonts::Initialize();

  if (look == nullptr)
    look = new Look();

  look->Initialise(Fonts::map);
}

void
MainWindow::InitialiseConfigured()
{
  const UISettings &ui_settings = CommonInterface::GetUISettings();

  if ((ui_settings.scale != 100) || (ui_settings.info_boxes.scale_title_font != 100) || (ui_settings.custom_dpi != 0))
    /* call Initialise() again to reload fonts with the new scale */
    Initialise();

  PixelRect rc = GetClientRect();

  const bool is_rounded =
    ui_settings.info_boxes.border_style == InfoBoxSettings::BorderStyle::OVERLAY;
  PixelRect ib_rc = rc;
  if (is_rounded)
    ib_rc.Grow(-Layout::Scale(2));

  const InfoBoxLayout::Layout ib_layout =
    InfoBoxLayout::Calculate(ib_rc, ui_settings.info_boxes.geometry,
                             GetInfoBoxRowHeightPercent());

  assert(look != nullptr);
  look->InitialiseConfigured(CommonInterface::GetUISettings(),
                             Fonts::map, Fonts::map_bold,
                             ib_layout.control_size.width);

  /* overlay_rect = area for popups/buttons (non-InfoBox area); in OVERLAY
     mode use the un-shrunk remaining area so overlays stay out of the
     floating boxes */
  const InfoBoxLayout::Layout overlay_layout = is_rounded
    ? InfoBoxLayout::Calculate(rc, ui_settings.info_boxes.geometry,
                               overlay_row_height_percent)
    : ib_layout;
  InfoBoxManager::Create(*this, ib_layout, look->info_box);
  map_rect = is_rounded ? rc : ib_layout.remaining;
  overlay_rect = is_rounded ? overlay_layout.remaining : map_rect;

  menu_bar = new MenuBar(*this, look->dialog.button);


  ReinitialiseLayout_vario(ib_layout);
  ReinitialiseLayoutTA(rc, ib_layout);
  ReinitialiseLayout_flarm(rc, ib_layout);

  ReinitialiseMapOverlayButtons();

#ifdef ANDROID
  /* create a rotate button (initially hidden) when orientation is
     DEFAULT (not forced) and the system auto-rotate setting is
     enabled; the button appears temporarily when the Java
     OrientationEventListener detects a physical orientation change */
  const UISettings &settings = CommonInterface::GetUISettings();
  const PixelRect map_area_rect = GetMapAreaRect();
  if (settings.display.orientation == DisplayOrientation::DEFAULT &&
      native_view != nullptr &&
      native_view->IsAutoRotateEnabled(Java::GetEnv())) {
    show_rotate_button = new ShowRotateButton();
    show_rotate_button->Create(*this, GetShowRotateButtonRect(map_area_rect));
    show_rotate_button->Hide();
  }
#endif

  map = new GlueMapWindow(*look);
  map->SetComputerSettings(CommonInterface::GetComputerSettings());
  map->SetMapSettings(CommonInterface::GetMapSettings());
  map->SetUIState(CommonInterface::GetUIState());
  const PixelRect map_position = GetMapDisplayRect(map_rect);
  map->Create(*this, map_position);
  map->SetContentRect(GetMapContentRect());

  popup = new PopupMessage(*this, look->dialog, ui_settings);
  popup->Create(overlay_rect);
}

void
MainWindow::InitialiseStorage() noexcept
{
  if (backend_components == nullptr ||
      backend_components->storage_manager == nullptr)
    return;

  /* Create a small adapter that forwards storage events to our
     private OnStorageEvent() method and register it directly
     with the StorageManager. */
  class Adapter final : public StorageEventListener {
    MainWindow &window_;
  public:
    explicit Adapter(MainWindow &w) noexcept : window_(w) {}
    void OnStorageEvent(const StorageEventInfo &info) noexcept override {
      window_.OnStorageEvent(info);
    }
  };

  storage_event_adapter_ = std::make_unique<Adapter>(*this);
  backend_components->storage_manager->AddEventListener(
    *storage_event_adapter_);
}

void
MainWindow::DeinitialiseStorage() noexcept
{
  if (storage_event_adapter_ &&
      backend_components != nullptr &&
      backend_components->storage_manager != nullptr)
    backend_components->storage_manager->RemoveEventListener(
      *storage_event_adapter_);

  storage_event_adapter_.reset();
}

void
MainWindow::Deinitialise() noexcept
{
  InfoBoxManager::Destroy();

  delete menu_bar;
  menu_bar = nullptr;

  delete popup;
  popup = nullptr;

  // During destruction of GlueMapWindow WM_SETFOCUS gets called for
  // MainWindow which tries to set the focus to GlueMapWindow. Prevent
  // this issue by setting map to nullptr before calling delete.
  GlueMapWindow *temp_map = map;
  map = nullptr;
  delete temp_map;

  delete show_menu_button;
  show_menu_button = nullptr;
  delete show_quickmenu_button;
  show_quickmenu_button = nullptr;
  delete show_zoom_out_button;
  show_zoom_out_button = nullptr;
  delete show_zoom_in_button;
  show_zoom_in_button = nullptr;

#ifdef ANDROID
  rotate_button_timer.Cancel();
  delete show_rotate_button;
  show_rotate_button = nullptr;
#endif

  vario.Clear();
  traffic_gauge.Clear();
  thermal_assistant.Clear();

  delete look;
  look = nullptr;
}

void
MainWindow::ReinitialiseLayout_vario(const InfoBoxLayout::Layout &layout) noexcept
{
  if (!layout.HasVario()) {
    vario.Clear();
    return;
  }

  if (!vario.IsDefined())
    vario.Set(new GlueGaugeVario(CommonInterface::GetLiveBlackboard(),
                                 look->vario));

  vario.Move(layout.vario);
  vario.Show();

  // XXX vario->BringToTop();
}

/**
 * Shift the rectangle so it lies within the given bounds, keeping its
 * size.
 */
static void
ShiftRectInto(PixelRect &rc, const PixelRect &bounds) noexcept
{
  if (rc.right > bounds.right)
    rc.Offset(bounds.right - rc.right, 0);
  if (rc.left < bounds.left)
    rc.Offset(bounds.left - rc.left, 0);
  if (rc.bottom > bounds.bottom)
    rc.Offset(0, bounds.bottom - rc.bottom);
  if (rc.top < bounds.top)
    rc.Offset(0, bounds.top - rc.top);
}

void
MainWindow::ReinitialiseLayoutTA(PixelRect rc,
                                 const InfoBoxLayout::Layout &layout) noexcept
{
  unsigned sz = std::min(layout.control_size.height,
                         layout.control_size.width) * 2;
  unsigned mw = std::min((GetMainRect().bottom - GetMainRect().top),
                         (GetMainRect().right - GetMainRect().left));
  unsigned dia = std::min(sz, mw / 2);

  switch (CommonInterface::GetUISettings().thermal_assistant_position) {
  case (UISettings::ThermalAssistantPosition::BOTTOM_LEFT_AVOID_IB):
    rc.bottom = GetMainRect().bottom;
    rc.left = GetMainRect().left;
    rc.right = rc.left + dia;
    break;
  case (UISettings::ThermalAssistantPosition::BOTTOM_RIGHT_AVOID_IB):
    rc.bottom = GetMainRect().bottom;
    rc.right = GetMainRect().right;
    rc.left = rc.right - dia;
    break;
  case (UISettings::ThermalAssistantPosition::BOTTOM_RIGHT):
    rc.right = GetMainRect().right;
    rc.left = rc.right - dia;
    break;
  case (UISettings::ThermalAssistantPosition::TOP_LEFT):
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break;
  case (UISettings::ThermalAssistantPosition::TOP_RIGHT):
    rc.left = rc.right - dia;
    rc.bottom = rc.top + dia;
    break;
  case (UISettings::ThermalAssistantPosition::CENTER_TOP):
    rc.left = (rc.left + rc.right - dia) / 2 - 1;
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break;
  case (UISettings::ThermalAssistantPosition::TOP_LEFT_AVOID_IB):
    rc.top = GetMainRect().top;
    rc.left = GetMainRect().left;
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break;
  case (UISettings::ThermalAssistantPosition::TOP_RIGHT_AVOID_IB):
    rc.top = GetMainRect().top;
    rc.right = GetMainRect().right;
    rc.left = rc.right - dia;
    rc.bottom = rc.top + dia;
    break;
  case (UISettings::ThermalAssistantPosition::CENTER_TOP_AVOID_IB):
    rc.top = GetMainRect().top;
    rc.left = (GetMainRect().left + GetMainRect().right - dia) / 2 - 1;
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break; 
  default: // BOTTOM_LEFT
    rc.left = GetMainRect().left;
    rc.right = rc.left + dia;
    break;
  }
  rc.top = rc.bottom - dia;

  if (CommonInterface::GetUISettings().info_boxes.border_style ==
      InfoBoxSettings::BorderStyle::OVERLAY)
    /* keep the gauge inside the non-InfoBox area */
    ShiftRectInto(rc, GetOverlayFreeRect());

  thermal_assistant.Move(rc);
}

void
MainWindow::ReinitialiseLayout() noexcept
{
  if (map == nullptr)
    /* without the MapWindow, it is safe to assume that the MainWindow
       is just being initialized, and the InfoBoxes aren't initialized
       yet either, so there is nothing to do here */
    return;

  const PixelRect rc = GetClientRect();

#ifndef ENABLE_OPENGL
  if (draw_thread == nullptr)
    /* no layout changes during startup */
    return;
#endif

  InfoBoxManager::Destroy();

  const UISettings &ui_settings = CommonInterface::GetUISettings();

  const bool is_rounded =
    ui_settings.info_boxes.border_style == InfoBoxSettings::BorderStyle::OVERLAY;
  PixelRect ib_rc = rc;
  if (is_rounded)
    ib_rc.Grow(-Layout::Scale(2));

  const InfoBoxLayout::Layout ib_layout =
    InfoBoxLayout::Calculate(ib_rc, ui_settings.info_boxes.geometry,
                             GetInfoBoxRowHeightPercent());

  look->ReinitialiseLayout(ib_layout.control_size.width, ui_settings.info_boxes.scale_title_font);

  const InfoBoxLayout::Layout overlay_layout2 = is_rounded
    ? InfoBoxLayout::Calculate(rc, ui_settings.info_boxes.geometry,
                               overlay_row_height_percent)
    : ib_layout;
  InfoBoxManager::Create(*this, ib_layout, look->info_box);
  InfoBoxManager::ProcessTimer();
  map_rect = is_rounded ? rc : ib_layout.remaining;
  overlay_rect = is_rounded ? overlay_layout2.remaining : map_rect;

  if (popup != nullptr)
    popup->UpdateLayout(GetOverlayFreeRect());

  ReinitialiseLayout_vario(ib_layout);

  ReinitialiseLayout_flarm(rc, ib_layout);

  ReinitialiseLayoutTA(rc, ib_layout);

  if (map != nullptr) {
    if (FullScreen)
      InfoBoxManager::Hide();
    else
      InfoBoxManager::Show();

    LayoutMapArea();
    map->SetContentRect(GetMapContentRect());
    map->FullRedraw();
  }

  if (widget != nullptr)
    widget->Move(GetMainRect(rc));

  UpdateMapOverlayButtonLayout();

  if (map != nullptr)
    map->BringToBottom();

  /* the area covered by the map may have shrunk (e.g. when leaving the
     OVERLAY theme); repaint the display margins */
  Invalidate();
}

void
MainWindow::ReinitialiseLayout_flarm(PixelRect rc,
                                     const InfoBoxLayout::Layout &ib_layout) noexcept
{
  TrafficSettings::GaugeLocation val =
    CommonInterface::GetUISettings().traffic.gauge_location;

  // Automatic mode - follow info boxes
  if (val == TrafficSettings::GaugeLocation::AUTO) {
    switch (InfoBoxManager::layout.geometry) {
    case InfoBoxSettings::Geometry::TOP_LEFT_8:
    case InfoBoxSettings::Geometry::TOP_LEFT_12:
      if (InfoBoxManager::layout.landscape)
        val = TrafficSettings::GaugeLocation::BOTTOM_LEFT;
      else
        val = TrafficSettings::GaugeLocation::TOP_RIGHT;
      break;

    default:
      val = TrafficSettings::GaugeLocation::BOTTOM_RIGHT;    // Assume bottom right unles...
      break;
    }
  }

  unsigned sz = std::min(ib_layout.control_size.height,
                         ib_layout.control_size.width) * 2;
  unsigned mw = std::min((GetMainRect().bottom - GetMainRect().top),
                         (GetMainRect().right - GetMainRect().left));
  unsigned dia = std::min(sz, mw / 2);

  switch (val) {
  case TrafficSettings::GaugeLocation::TOP_LEFT:
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break;

  case TrafficSettings::GaugeLocation::TOP_RIGHT:
    rc.left = rc.right - dia;
    rc.bottom = rc.top + dia;
    break;

  case TrafficSettings::GaugeLocation::BOTTOM_LEFT:
    rc.right = rc.left + dia;
    rc.top = rc.bottom - dia;
    break;

  case TrafficSettings::GaugeLocation::CENTER_TOP:
    rc.left = (rc.left + rc.right - dia) / 2 - 1;
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break;

  case TrafficSettings::GaugeLocation::CENTER_BOTTOM:
    rc.left = (rc.left + rc.right - dia) / 2 - 1;
    rc.right = rc.left + dia;
    rc.top = rc.bottom - dia;
    break;

  case TrafficSettings::GaugeLocation::TOP_LEFT_AVOID_IB:
    rc.top = GetMainRect().top;
    rc.left = GetMainRect().left;
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break;

  case TrafficSettings::GaugeLocation::TOP_RIGHT_AVOID_IB:
    rc.top = GetMainRect().top;
    rc.right = GetMainRect().right;
    rc.left = rc.right - dia;
    rc.bottom = rc.top + dia;
    break;

  case TrafficSettings::GaugeLocation::BOTTOM_LEFT_AVOID_IB:
    rc.bottom = GetMainRect().bottom;
    rc.left = GetMainRect().left;
    rc.right = rc.left + dia;
    rc.top = rc.bottom - dia;
    break;

  case TrafficSettings::GaugeLocation::CENTER_TOP_AVOID_IB:
    rc.top = GetMainRect().top;
    rc.left = (GetMainRect().left + GetMainRect().right - dia) / 2 - 1;
    rc.right = rc.left + dia;
    rc.bottom = rc.top + dia;
    break;

  case TrafficSettings::GaugeLocation::CENTER_BOTTOM_AVOID_IB:
    rc.bottom = GetMainRect().bottom;
    rc.left = (GetMainRect().left + GetMainRect().right - dia) / 2 - 1;
    rc.right = rc.left + dia;
    rc.top = rc.bottom - dia;
    break;

  case TrafficSettings::GaugeLocation::BOTTOM_RIGHT_AVOID_IB:
    rc.bottom = GetMainRect().bottom;
    rc.right = GetMainRect().right;
    rc.left = rc.right - dia;
    rc.top = rc.bottom - dia;
    break;

  default:    // aka flBottomRight
    rc.left = rc.right - dia;
    rc.top = rc.bottom - dia;
    break;
  }

  ++rc.top;
  ++rc.left;

  if (CommonInterface::GetUISettings().info_boxes.border_style ==
      InfoBoxSettings::BorderStyle::OVERLAY)
    /* keep the gauge inside the non-InfoBox area */
    ShiftRectInto(rc, GetOverlayFreeRect());

  traffic_gauge.Move(rc);
}

void
MainWindow::ReinitialiseLook() noexcept
{
  const auto &ui_settings = CommonInterface::GetUISettings();

  const InfoBoxLayout::Layout ib_layout =
    InfoBoxLayout::Calculate(GetClientRect(),
                             ui_settings.info_boxes.geometry,
                             GetInfoBoxRowHeightPercent());

  assert(look != nullptr);
  look->InitialiseConfigured(CommonInterface::GetUISettings(),
                             Fonts::map, Fonts::map_bold,
                             ib_layout.control_size.width);

  InfoBoxManager::ScheduleRedraw();
}

#ifdef ANDROID

void
MainWindow::OnLook() noexcept
{
  ReinitialiseLook();
}

void
MainWindow::OnTaskReceived() noexcept
{
  if (!IsRunning())
    /* postpone until XCSoar is running */
    return;

  if (HasDialog())
    /* don't intercept an existing modal dialog */
    return;

  auto task = GetReceivedTask();
  if (!task)
    return;

  dlgTaskManagerShowModal(std::move(task));
}

#endif // ANDROID

void
MainWindow::Destroy() noexcept
{
  Deinitialise();

  TopWindow::Destroy();
}

void
MainWindow::FinishStartup() noexcept
{
  timer.Schedule(std::chrono::milliseconds(500)); // 2 times per second

  ResumeThreads();
}

void
MainWindow::BeginShutdown() noexcept
{
  timer.Cancel();

  refresh_info_boxes_pending = false;
  page_actions_update_pending = false;
  refresh_info_boxes_notify.ClearNotification();
  page_actions_update_notify.ClearNotification();

  KillTopWidget();
  KillBottomWidget();
}

void
MainWindow::SuspendThreads() noexcept
{
  if (map != nullptr)
    map->SuspendThreads();
}

void
MainWindow::ResumeThreads() noexcept
{
  if (map != nullptr)
    map->ResumeThreads();
}

void
MainWindow::SetDefaultFocus() noexcept
{
  if (map != nullptr && widget == nullptr)
    map->SetFocus();
  else if (widget == nullptr || !widget->SetFocus())
    SetFocus();
}

void
MainWindow::FlushRendererCaches() noexcept
{
  if (map != nullptr)
    map->FlushCaches();
}

void
MainWindow::FullRedraw() noexcept
{
  if (map != nullptr)
    map->FullRedraw();
}

void
MainWindow::OnStorageNotify() noexcept
{
  if (backend_components == nullptr ||
      backend_components->storage_manager == nullptr)
    return;

  backend_components->storage_manager->ProcessPendingChanges();
}

void
MainWindow::OnStorageEvent(const StorageEventInfo &info) noexcept
{
  /* Show a popup only when the map is active and no dialog is
     currently open.  This avoids queueing stale storage popups while
     a modal dialog is shown and replaying them afterwards. */
  if (GetMapIfActive() == nullptr || HasDialog())
    return;

  if (!popup)
    return;

  const std::string msg = info.Format();
  if (!msg.empty())
    popup->AddMessage(msg.c_str());
}

// Windows event handlers

#ifdef USE_WINUSER
LRESULT
MainWindow::OnMessage(HWND hWnd, UINT message,
                      WPARAM wParam, LPARAM lParam) noexcept
{
  switch (message) {
  case WM_DEVICECHANGE:
    /* Forward device change notifications to the storage hotplug
       forwarder which will call the registered
       WindowsStorageHotplugMonitor. */
    Storage::Win::ForwardDeviceChange(wParam, lParam);
    break;
  }

  return SingleWindow::OnMessage(hWnd, message, wParam, lParam);
}
#endif

void
MainWindow::OnResize(PixelSize new_size) noexcept
{
  Layout::Initialise(GetDisplay(), new_size,
                     CommonInterface::GetUISettings().GetPercentScale(),
                     CommonInterface::GetUISettings().custom_dpi);

  SingleWindow::OnResize(new_size);

  ReinitialiseLayout();

  const PixelRect rc = GetClientRect();

  if (menu_bar != nullptr)
    menu_bar->OnResize(rc);

  ProgressGlue::Move(rc);
}

void
MainWindow::OnSetFocus() noexcept
{
  SingleWindow::OnSetFocus();

  if (!HasDialog()) {
    /* the main window should never have the keyboard focus; if we
       happen to get the focus despite of that, forward it to the map
       window to make keyboard shortcuts work */
    if (map != nullptr && widget == nullptr)
      map->SetFocus();
    else if (widget != nullptr)
      widget->SetFocus();
  } else
    /* recover the dialog focus if it got lost */
    GetTopDialog().FocusFirstControl();
}

void
MainWindow::StopDragging() noexcept
{
  if (!dragging)
    return;

  dragging = false;
  ReleaseCapture();
}

void
MainWindow::OnCancelMode() noexcept
{
  SingleWindow::OnCancelMode();
  StopDragging();
}

bool
MainWindow::OnMouseDown(PixelPoint p) noexcept
{
  if (SingleWindow::OnMouseDown(p))
    return true;

  if (!dragging && !HasDialog()) {
    dragging = true;
    SetCapture();
    gestures.Start(p, Layout::Scale(20));
  }

  return true;
}

bool
MainWindow::OnMouseUp(PixelPoint p) noexcept
{
  if (SingleWindow::OnMouseUp(p))
    return true;

  if (dragging) {
    StopDragging();

    const char *gesture = gestures.Finish();
    if (gesture && InputEvents::processGesture(gesture))
      return true;
  }

  return false;
}

bool
MainWindow::OnMouseDouble(PixelPoint p) noexcept
{
  if (SingleWindow::OnMouseDouble(p))
    return true;

  StopDragging();

  if (!HasDialog())
    InputEvents::ShowMenu();
  return false;
}

bool
MainWindow::OnMouseMove(PixelPoint p, unsigned keys) noexcept
{
  if (SingleWindow::OnMouseMove(p, keys))
    return true;

  if (dragging)
    gestures.Update(p);

  return true;
}

bool
MainWindow::OnKeyDown(unsigned key_code) noexcept
{
  return (widget != nullptr && widget->KeyPress(key_code)) ||
    (HaveTopWidget() && top_widget->KeyPress(key_code)) ||
    (HaveBottomWidget() && bottom_widget->KeyPress(key_code)) ||
    InputEvents::processKey(key_code) ||
    SingleWindow::OnKeyDown(key_code);
}

inline void
MainWindow::LateInitialise() noexcept
{
  if (late_initialised)
    return;

  late_initialised = true;

  if (backend_components->devices != nullptr) {
    /* this OperationEnvironment instance must be persistent, because
       DeviceDescriptor::Open() is asynchronous */
    static PopupOperationEnvironment env;

    /* opening all devices needs to be postponed to here because
       during early initialisation (before the main event loop runs),
       opening some devices may be intercepted by Android which pauses
       XCSoar in order to ask the user for permission; pausing works
       properly only if the main event loop runs */
    backend_components->devices->Open(env);
  }
}

void
MainWindow::RunTimer() noexcept
{
  LateInitialise();

#ifdef ANDROID
  /* if we still havn't processed the task that was received from a QR
     code, re-post the TASK_RECEIVED event to invoke OnTaskReceived()
     again; we must not open the task manager dialog here because it
     would block the timer while the dialog is open */
  if (IsRunning() && !HasDialog() && HasReceivedTask())
    UI::event_queue->Inject(UI::Event::TASK_RECEIVED);
#endif

  ProcessTimer();

#ifdef ENABLE_OPENGL
  if (GlueMapWindow *m = GetMapIfActive())
    m->PollTerrainQuantisationIdle();
#endif

  UpdateGaugeVisibility();

  if (CommonInterface::GetUISettings().thermal_assistant_position == UISettings::ThermalAssistantPosition::OFF) {
    thermal_assistant.Clear();
  } else if (!CommonInterface::Calculated().circling ||
             InputEvents::IsFlavour("TA")) {
    thermal_assistant.Hide();
  } else if (!HasDialog()) {
    if (!thermal_assistant.IsDefined())
      thermal_assistant.Set(new GaugeThermalAssistant(CommonInterface::GetLiveBlackboard(),
                                                      look->thermal_assistant_gauge));

    if (!thermal_assistant.IsVisible()) {
      thermal_assistant.Show();

      GaugeThermalAssistant *widget =
        (GaugeThermalAssistant *)thermal_assistant.Get();
      widget->Raise();
    }
  }

  battery_timer.Process();
}

void
MainWindow::SendGPSUpdate(const bool vario_bar_redraw) noexcept
{
  vario_bar_redraw_pending = vario_bar_redraw;
  gps_notify.SendNotification();
}

void
MainWindow::OnGpsNotify() noexcept
{
  PopupOperationEnvironment env;
  UIReceiveSensorData(env);

  if (std::exchange(vario_bar_redraw_pending, false) &&
      CommonInterface::GetMapSettings().vario_bar_enabled) {
    if (GlueMapWindow *m = GetMapIfActive())
      m->InjectRedraw();
  }
}

void
MainWindow::OnCalculatedNotify() noexcept
{
  UIReceiveCalculatedData();
}

void
MainWindow::OnRefreshInfoBoxesNotify() noexcept
{
  refresh_info_boxes_pending = false;

  if (!InfoBoxManager::IsReady())
    return;

  InfoBoxManager::SetDirty();
  InfoBoxManager::ProcessTimer();
  SetUIState(CommonInterface::GetUIState());
}

void
MainWindow::ScheduleRefreshInfoBoxes() noexcept
{
  if (refresh_info_boxes_pending)
    return;

  refresh_info_boxes_pending = true;
  refresh_info_boxes_notify.SendNotification();
}

void
MainWindow::OnPageActionsUpdateNotify() noexcept
{
  page_actions_update_pending = false;
  PageActions::Update();
}

void
MainWindow::SchedulePageActionsUpdate() noexcept
{
  if (page_actions_update_pending)
    return;

  page_actions_update_pending = true;
  page_actions_update_notify.SendNotification();
}

void
MainWindow::OnRestorePageNotify() noexcept
{
  if (restore_page_pending)
    PageActions::Restore();
}

#ifdef ANDROID
void
MainWindow::OnRotationSuggestion() noexcept
{
  if (show_rotate_button == nullptr)
    return;

  show_rotate_button->Show();
  rotate_button_timer.Schedule(std::chrono::seconds{5});
}

void
MainWindow::OnRotateButtonTimeout() noexcept
{
  if (show_rotate_button != nullptr)
    show_rotate_button->Hide();
}
#endif

void
MainWindow::OnDestroy() noexcept
{
  timer.Cancel();

  KillWidget();
  KillTopWidget();
  KillBottomWidget();

  SingleWindow::OnDestroy();
}

bool
MainWindow::OnClose() noexcept
{
  if (HasDialog() || !IsRunning())
    /* no shutdown dialog if XCSoar hasn't completed initialization
       yet (e.g. if we are in the simulator prompt) */
    return SingleWindow::OnClose();

  if (UIActions::CheckShutdown()) {
    PostQuit();
  }
  return true;
}

void
MainWindow::OnPaint(Canvas &canvas) noexcept
{
  /* Clear the display margins outside the safe area (notch, home
     indicator).  Only the OVERLAY theme draws the map into them; in
     all other themes nothing paints there, so without this the pixels
     of the previously shown theme would remain visible. */
  const PixelRect display_rc = GetDisplayRect();
  PixelRect covered = GetClientRect();
  if (map != nullptr) {
    const PixelRect map_rc = map->GetPosition();
    covered.left = std::min(covered.left, map_rc.left);
    covered.top = std::min(covered.top, map_rc.top);
    covered.right = std::max(covered.right, map_rc.right);
    covered.bottom = std::max(covered.bottom, map_rc.bottom);
  }

  if (display_rc.top < covered.top)
    canvas.DrawFilledRectangle({display_rc.left, display_rc.top,
                                display_rc.right, covered.top},
                               COLOR_BLACK);
  if (covered.bottom < display_rc.bottom)
    canvas.DrawFilledRectangle({display_rc.left, covered.bottom,
                                display_rc.right, display_rc.bottom},
                               COLOR_BLACK);
  if (display_rc.left < covered.left)
    canvas.DrawFilledRectangle({display_rc.left, covered.top,
                                covered.left, covered.bottom},
                               COLOR_BLACK);
  if (covered.right < display_rc.right)
    canvas.DrawFilledRectangle({covered.right, covered.top,
                                display_rc.right, covered.bottom},
                               COLOR_BLACK);

  if (HaveTopWidget() && map != nullptr) {
    /* draw a separator between top widget and map */
    PixelRect rc = map->GetPosition();
    rc.bottom = rc.top;
    rc.top -= separator_height;
    canvas.DrawFilledRectangle(rc, COLOR_BLACK);
  }

  if (HaveBottomWidget() && map != nullptr) {
    /* draw a separator between main area and bottom area */
    PixelRect rc = map->GetPosition();
    rc.top = rc.bottom;
    rc.bottom += separator_height;
    canvas.DrawFilledRectangle(rc, COLOR_BLACK);
  }

  SingleWindow::OnPaint(canvas);
}

void
MainWindow::SetFullScreen(bool _full_screen) noexcept
{
  if (_full_screen == FullScreen)
    return;

  FullScreen = _full_screen;

  if (FullScreen)
    InfoBoxManager::Hide();
  else
    InfoBoxManager::Show();

  if (widget != nullptr)
    widget->Move(GetMainRect());

  /* Overlapped gauges (FLARM, thermal assistant) use GetMainRect() for
     "avoid InfoBoxes" corners; re-layout when fullscreen changes. */
  const PixelRect rc = GetClientRect();
  const InfoBoxLayout::Layout ib_layout =
    InfoBoxLayout::Calculate(rc,
                             CommonInterface::GetUISettings().info_boxes.geometry,
                             GetInfoBoxRowHeightPercent());
  ReinitialiseLayout_flarm(rc, ib_layout);
  ReinitialiseLayoutTA(rc, ib_layout);

  if (map != nullptr) {
    LayoutMapArea();
    UpdateMapOverlayButtonLayout();

    map->SetContentRect(GetMapContentRect());
  }

  if (popup != nullptr)
    popup->UpdateLayout(GetOverlayFreeRect());

  // the repaint will be triggered by the DrawThread

  UpdateVarioGaugeVisibility();
}

void
MainWindow::SetTerrain(RasterTerrain *terrain) noexcept
{
  if (map != nullptr)
    map->SetTerrain(terrain);
}

void
MainWindow::SetTopography(TopographyStore *topography) noexcept
{
  if (map != nullptr)
    map->SetTopography(topography);
}

void
MainWindow::SetComputerSettings(const ComputerSettings &settings_computer) noexcept
{
  if (map != nullptr)
    map->SetComputerSettings(settings_computer);
}

void
MainWindow::SetMapSettings(const MapSettings &settings_map) noexcept
{
  if (map != nullptr)
    map->SetMapSettings(settings_map);
}

void
MainWindow::SetUIState(const UIState &ui_state) noexcept
{
  if (map != nullptr) {
    map->SetUIState(ui_state);
    map->FullRedraw();
  }
}

GlueMapWindow *
MainWindow::GetMapIfActive() noexcept
{
  return IsMapActive() ? map : nullptr;
}

GlueMapWindow *
MainWindow::ActivateMap() noexcept
{
  restore_page_pending = false;

  if (map == nullptr)
    return nullptr;

  if (widget != nullptr) {
    KillWidget();

    if (bottom_widget != nullptr) {
      PixelRect main_rect = GetWidgetAreaRect();
      const PixelRect top_rect = GetTopWidgetRect(main_rect, top_widget);
      main_rect = GetMapRectBelow(main_rect, top_rect);
      bottom_widget->Show(GetBottomWidgetRect(main_rect, bottom_widget));
    }

    LayoutMapArea();
    map->Show();
    map->SetFocus();
    UpdateMapOverlayButtonLayout();

#ifndef ENABLE_OPENGL
    if (draw_suspended) {
      draw_suspended = false;
      draw_thread->Resume();
    }
#endif
  }

  return map;
}

void
MainWindow::DeferredRestorePage() noexcept
{
  if (restore_page_pending)
    return;

  restore_page_pending = true;
  restore_page_notify.SendNotification();
}

void
MainWindow::KillWidget() noexcept
{
  if (widget == nullptr)
    return;

  /* Clear the pointer before destroying.  On Windows, DestroyWindow on
     a focused child (e.g. FLARM radar) can re-enter OnSetFocus(); if
     #widget still pointed here, WindowWidget::SetFocus() would assert
     after the native window was already destroyed (#2824). */
  Widget *const old = widget;
  widget = nullptr;
  InputEvents::SetFlavour(nullptr);

  old->Leave();
  old->Hide();
  old->Unprepare();
  delete old;
}

void
MainWindow::KillTopWidget() noexcept
{
  if (top_widget == nullptr)
    return;

  Widget *const old = top_widget;
  top_widget = nullptr;

  old->Hide();
  old->Unprepare();
  delete old;
}

void
MainWindow::SetTopWidget(Widget *_widget) noexcept
{
  if (top_widget == nullptr && _widget == nullptr)
    return;

  KillTopWidget();

  top_widget = _widget;

  PixelRect main_rect = GetWidgetAreaRect();
  const PixelRect top_rect = GetTopWidgetRect(main_rect,
                                              top_widget);
  if (top_widget != nullptr) {
    top_widget->Initialise(*this, top_rect);
    top_widget->Prepare(*this, top_rect);
    top_widget->Show(top_rect);
  }

  LayoutMapArea();
  map->FullRedraw();

  UpdateMapOverlayButtonLayout();
}

void
MainWindow::KillBottomWidget() noexcept
{
  if (bottom_widget == nullptr)
    return;

  Widget *const old = bottom_widget;
  bottom_widget = nullptr;

  if (widget == nullptr)
    /* the bottom widget is only visible below the map, but not below
       a custom main widget; see HaveBottomWidget() */
    old->Hide();

  old->Unprepare();
  delete old;
}

void
MainWindow::SetBottomWidget(Widget *_widget) noexcept
{
  if (bottom_widget == nullptr && _widget == nullptr)
    return;

  if (map == nullptr) {
    /* this doesn't work without a map */
    delete _widget;
    return;
  }

  KillBottomWidget();

  bottom_widget = _widget;

  PixelRect main_rect = GetWidgetAreaRect();
  const PixelRect top_rect = GetTopWidgetRect(main_rect,
                                              top_widget);
  main_rect = GetMapRectBelow(main_rect, top_rect);
  if (HaveTopWidget())
    top_widget->Move(top_rect);

  if (bottom_widget != nullptr) {
    /*
     * Prepare the bottom widget with the full available main area
     * first, so it can create child controls and report its final
     * minimum size before GetBottomWidgetRect() computes the actual
     * bottom rectangle.
     */
    bottom_widget->Initialise(*this, main_rect);
    bottom_widget->Prepare(*this, main_rect);
  }

  const PixelRect bottom_rect = GetBottomWidgetRect(main_rect,
                                                    bottom_widget);

  if (bottom_widget != nullptr) {
    if (widget == nullptr)
      /* the bottom widget is only visible below the map, but not
         below a custom main widget; see HaveBottomWidget() */
      bottom_widget->Show(bottom_rect);
    /* else: leave hidden until ActivateMap() shows it */
  }

  LayoutMapArea();
  map->FullRedraw();

  UpdateMapOverlayButtonLayout();
}

void
MainWindow::SetWidget(Widget *_widget) noexcept
{
  assert(_widget != nullptr);

  restore_page_pending = false;

  const bool have_bottom_widget = HaveBottomWidget();

  /* delete the old widget */
  KillWidget();

  /* hide the map (might be hidden already) */
  if (map != nullptr) {
    map->FastHide();

#ifndef ENABLE_OPENGL
    if (!draw_suspended) {
      draw_suspended = true;
      draw_thread->BeginSuspend();
    }
#endif
  }

  if (have_bottom_widget)
    bottom_widget->Hide();

  widget = _widget;

  const PixelRect rc = GetMainRect();
  widget->Initialise(*this, rc);
  widget->Prepare(*this, rc);
  widget->Show(rc);

  UpdateMapOverlayButtonLayout();

  if (!widget->SetFocus())
    SetFocus();
}

Widget *
MainWindow::GetFlavourWidget(const char *flavour) noexcept
{
  return InputEvents::IsFlavour(flavour)
    ? widget
    : nullptr;
}

void
MainWindow::ShowMenu(const Menu &menu, const Menu *overlay, bool full) noexcept
{
  assert(menu_bar != nullptr);

  MenuGlue::Set(*menu_bar, menu, overlay, full);
}

bool
MainWindow::IsMenuButtonEnabled(unsigned idx) noexcept
{
  assert(menu_bar != nullptr);

  return menu_bar->IsButtonEnabled(idx);
}

void
MainWindow::UpdateVarioGaugeVisibility() noexcept
{
  bool full_screen = GetFullScreen();

  vario.SetVisible(!full_screen &&
                   !CommonInterface::GetUIState().screen_blanked);
}

void
MainWindow::UpdateGaugeVisibility() noexcept
{
  UpdateVarioGaugeVisibility();
  UpdateTrafficGaugeVisibility();
}

void
MainWindow::UpdateTrafficGaugeVisibility() noexcept
{
  const FlarmData &flarm = CommonInterface::Basic().flarm;

  bool traffic_visible =
    (force_traffic_gauge ||
     (CommonInterface::GetUISettings().traffic.enable_gauge &&
      !flarm.traffic.IsEmpty())) &&
    !CommonInterface::GetUIState().screen_blanked &&
    /* hide the traffic gauge while the traffic widget is visible, to
       avoid showing the same information twice */
    !InputEvents::IsFlavour("Traffic");

  if (traffic_visible && suppress_traffic_gauge) {
    if (flarm.status.available &&
        flarm.status.alarm_level != FlarmTraffic::AlarmType::NONE)
      suppress_traffic_gauge = false;
    else
      traffic_visible = false;
  }

  if (traffic_visible) {
    if (HasDialog())
      return;

    if (!flarm.traffic.InCloseRange()) {
      traffic_gauge.Hide();
      return;
    }

    if (!traffic_gauge.IsDefined())
      traffic_gauge.Set(new GaugeFLARM(CommonInterface::GetLiveBlackboard(),
                                       GetLook().flarm_gauge));

    if (!traffic_gauge.IsVisible()) {
      traffic_gauge.Show();

      GaugeFLARM *widget = (GaugeFLARM *)traffic_gauge.Get();
      widget->Raise();
    }
  } else
    traffic_gauge.Hide();
}

const MapWindowProjection &
MainWindow::GetProjection() const noexcept
{
  AssertThread();
  assert(map != nullptr);

  return map->VisibleProjection();
}

void
MainWindow::ToggleSuppressFLARMRadar() noexcept
{
  suppress_traffic_gauge = !suppress_traffic_gauge;
}

void
MainWindow::ToggleForceFLARMRadar() noexcept
{
  force_traffic_gauge = !force_traffic_gauge;
  CommonInterface::SetUISettings().traffic.enable_gauge = force_traffic_gauge;
}
