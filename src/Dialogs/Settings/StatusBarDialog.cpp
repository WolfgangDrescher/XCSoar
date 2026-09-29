// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "StatusBarDialog.hpp"
#include "Dialogs/ComboPicker.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Form/Button.hpp"
#include "Form/ButtonPanel.hpp"
#include "Form/DataField/ComboList.hpp"
#include "Form/DataField/Enum.hpp"
#include "InfoBoxes/Content/Factory.hpp"
#include "Language/Language.hpp"
#include "Look/DialogLook.hpp"
#include "MainWindow.hpp"
#include "Profile/Current.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Map.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "Screen/Layout.hpp"
#include "ui/canvas/Canvas.hpp"
#include "StatusBarItems.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "UISettings.hpp"
#include "ui/window/Features.hpp" // for HAVE_SYSTEM_STATUS_BAR_SETTING
#include "Widget/ButtonPanelWidget.hpp"
#include "Widget/ListWidget.hpp"
#include "Widget/RowFormWidget.hpp"
#include "util/StaticString.hxx"
#include "util/StringAPI.hxx"

#include <algorithm>
#include <cassert>
#include <vector>

using Symbol = StatusBarItem::Symbol;
using Zone = StatusBarItem::Zone;

static constexpr const char *zone_names[] = {
  N_("Left"),
  N_("Right"),
};

static_assert(std::size(zone_names) == unsigned(Zone::COUNT));

static constexpr const char *symbol_names[] = {
  N_("Battery"),
  N_("GPS"),
  N_("FLARM"),
  N_("Ballast"),
  N_("Logger"),
  N_("Network"),
  N_("Page number"),
  N_("Page title"),
  N_("Data source"),
};

static_assert(std::size(symbol_names) == unsigned(Symbol::COUNT));

static constexpr const char *symbol_help[] = {
  N_("The charge of the battery, written inside it; green while "
     "charging, red when low."),
  N_("An arrow: filled with a fix, yellow while waiting for one, and "
     "red without a GPS."),
  N_("A glider with the number of aircraft around, red or amber during "
     "an alarm."),
  N_("A drop with the litres left, during a ballast dump."),
  N_("A red dot while the logger records."),
  N_("A cloud, grey without an internet connection."),
  N_("The position of the page, e.g. 2/5."),
  N_("The short title of the page."),
  N_("Simulator or replay, if the data does not come from the GPS."),
};

static_assert(std::size(symbol_help) == unsigned(Symbol::COUNT));

/**
 * The name of an item in the list, e.g. "GPS" or "InfoBox: Time
 * local".
 */
static void
FormatItemName(StaticString<96> &buffer, StatusBarItem item) noexcept
{
  switch (item.kind) {
  case StatusBarItem::Kind::SYMBOL:
    buffer = gettext(symbol_names[item.id]);
    return;

  case StatusBarItem::Kind::INFOBOX: {
    const auto type = InfoBoxFactory::Type(item.id);
    buffer.Format("%s: %s", _("InfoBox"),
                  gettext(InfoBoxFactory::GetName(type)));
    return;
  }
  }

  buffer.clear();
}

/**
 * Let the user pick an InfoBox, in alphabetical order.
 *
 * @return false if the user has cancelled
 */
static bool
PickInfoBox(StatusBarItem &item) noexcept
{
  std::vector<InfoBoxFactory::Type> types;
  for (unsigned i = InfoBoxFactory::MIN_TYPE_VAL;
       i < InfoBoxFactory::NUM_TYPES; ++i)
    types.push_back(InfoBoxFactory::Type(i));
  std::sort(types.begin(), types.end(), [](auto a, auto b){
    return StringCollate(gettext(InfoBoxFactory::GetName(a)),
                         gettext(InfoBoxFactory::GetName(b))) < 0;
  });

  ComboList list;
  for (const auto type : types) {
    const char *name = gettext(InfoBoxFactory::GetName(type));
    const char *help = InfoBoxFactory::GetDescription(type);
    list.Append(type, name, name,
                help != nullptr ? gettext(help) : nullptr);
  }

  const int result = ComboPicker(_("InfoBox"), list, nullptr, true);
  if (result < 0)
    return false;

  item = StatusBarItem::Make(item.zone,
                             InfoBoxFactory::Type(list[result].int_value));
  return true;
}

/**
 * Let the user pick an item: a symbol, or an InfoBox from a second
 * list.
 *
 * @return false if the user has cancelled
 */
static bool
PickItem(StatusBarItem &item) noexcept
{
  ComboList list;
  for (unsigned i = 0; i < unsigned(Symbol::COUNT); ++i)
    list.Append(i, gettext(symbol_names[i]), gettext(symbol_names[i]),
                gettext(symbol_help[i]));

  static constexpr int INFOBOX = -1;
  list.Append(INFOBOX, _("InfoBox"), _("InfoBox"),
              _("The value of an InfoBox, e.g. the local time."));

  const int result = ComboPicker(_("Status bar"), list, nullptr, true);
  if (result < 0)
    return false;

  const int value = list[result].int_value;
  if (value == INFOBOX)
    return PickInfoBox(item);

  item = StatusBarItem::Make(item.zone, Symbol(value));
  return true;
}

/**
 * The items of the status bar, zone by zone: each zone has a header
 * row, followed by its items from left to right.
 */
class StatusBarListWidget final : public ListWidget {
  TextRowRenderer row_renderer;

  StatusBarItems &items;

  ButtonPanelWidget *buttons;
  Button *add_button = nullptr, *delete_button = nullptr;
  Button *move_up_button = nullptr, *move_down_button = nullptr;

public:
  explicit StatusBarListWidget(StatusBarItems &_items) noexcept
    :items(_items) {}

  void SetButtonPanel(ButtonPanelWidget &_buttons) noexcept {
    buttons = &_buttons;
  }

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;

private:
  unsigned GetRowCount() const noexcept {
    return items.n_items + unsigned(Zone::COUNT);
  }

  /**
   * The row of the given item: the items, with a header before
   * each zone.
   */
  unsigned GetItemRow(unsigned i) const noexcept {
    return i + unsigned(items.items[i].zone) + 1;
  }

  /**
   * The item in the given row.
   *
   * @return false if the row is the header of #zone, which is then
   * set
   */
  bool GetRowItem(unsigned row, unsigned &i, Zone &zone) const noexcept;

  void OnAdd() noexcept;
  void OnDelete() noexcept;
  void OnMove(bool up) noexcept;

  void SetCursorItem(unsigned i) noexcept {
    GetList().SetLength(GetRowCount());
    GetList().SetCursorIndex(GetItemRow(i));
    GetList().Invalidate();
    UpdateButtons();
  }

  void CreateButtons(ButtonPanel &panel) noexcept;
  void UpdateButtons() noexcept;

  /* virtual methods from class ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned idx) noexcept override;

  /* virtual methods from class ListCursorHandler */
  void OnCursorMoved(unsigned index) noexcept override;
};

bool
StatusBarListWidget::GetRowItem(unsigned row, unsigned &i,
                                Zone &zone) const noexcept
{
  for (unsigned z = 0; z < unsigned(Zone::COUNT); ++z) {
    const unsigned begin = items.GetZoneBegin(Zone(z));
    const unsigned header = begin + z;
    if (row == header) {
      zone = Zone(z);
      return false;
    }

    const unsigned n = items.GetZone(Zone(z)).size();
    if (row > header && row <= header + n) {
      i = begin + (row - header - 1);
      zone = Zone(z);
      return true;
    }
  }

  /* not reached */
  zone = Zone::LEFT;
  return false;
}

void
StatusBarListWidget::OnAdd() noexcept
{
  if (items.IsFull())
    return;

  /* insert after the selected item, or at the start of the selected
     zone */
  unsigned i;
  Zone zone;
  const unsigned position = GetRowItem(GetList().GetCursorIndex(), i, zone)
    ? i + 1
    : items.GetZoneBegin(zone);

  StatusBarItem item;
  item.zone = zone;
  if (!PickItem(item))
    return;

  items.Insert(position, item);
  SetCursorItem(position);
}

void
StatusBarListWidget::OnDelete() noexcept
{
  unsigned i;
  Zone zone;
  if (!GetRowItem(GetList().GetCursorIndex(), i, zone))
    return;

  const unsigned row = GetList().GetCursorIndex();
  items.Remove(i);
  GetList().SetLength(GetRowCount());
  GetList().SetCursorIndex(std::min(row, GetRowCount() - 1));
  GetList().Invalidate();
  UpdateButtons();
}

void
StatusBarListWidget::OnMove(bool up) noexcept
{
  unsigned i;
  Zone zone;
  if (!GetRowItem(GetList().GetCursorIndex(), i, zone))
    return;

  auto &item = items.items[i];
  if (up) {
    if (i > 0 && items.items[i - 1].zone == zone) {
      std::swap(item, items.items[i - 1]);
      --i;
    } else if (zone != Zone::LEFT)
      /* over the header: the last item of the zone before */
      item.zone = Zone(unsigned(zone) - 1);
    else
      return;
  } else {
    if (i + 1 < items.n_items && items.items[i + 1].zone == zone) {
      std::swap(item, items.items[i + 1]);
      ++i;
    } else if (unsigned(zone) + 1 < unsigned(Zone::COUNT))
      /* over the header: the first item of the zone after */
      item.zone = Zone(unsigned(zone) + 1);
    else
      return;
  }

  SetCursorItem(i);
}

void
StatusBarListWidget::CreateButtons(ButtonPanel &panel) noexcept
{
  add_button = panel.Add(C_("Button", "Add"), [this](){ OnAdd(); });
  delete_button = panel.Add(C_("Button", "Delete"),
                            [this](){ OnDelete(); });
  move_up_button = panel.AddSymbol("^", [this](){ OnMove(true); });
  move_down_button = panel.AddSymbol("v", [this](){ OnMove(false); });

  UpdateButtons();
}

void
StatusBarListWidget::UpdateButtons() noexcept
{
  if (add_button == nullptr)
    /* not created yet */
    return;

  const unsigned row = GetList().GetCursorIndex();
  unsigned i;
  Zone zone;
  const bool is_item = GetRowItem(row, i, zone);

  add_button->SetEnabled(!items.IsFull());
  delete_button->SetEnabled(is_item);
  /* the first row is a header, and so is the second if the left
     zone is empty */
  move_up_button->SetEnabled(is_item && row > 1);
  move_down_button->SetEnabled(is_item && row + 1 < GetRowCount());
}

void
StatusBarListWidget::Prepare(ContainerWindow &parent,
                             const PixelRect &rc) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  CreateList(parent, look, rc,
             row_renderer.CalculateLayout(*look.list.font))
    .SetLength(GetRowCount());

  CreateButtons(buttons->GetButtonPanel());

  /* start at the first item, not at a header */
  if (items.n_items > 0)
    GetList().SetCursorIndex(GetItemRow(0));
}

void
StatusBarListWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                                 unsigned idx) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  unsigned i;
  Zone zone;
  if (!GetRowItem(idx, i, zone)) {
    /* the header of a zone */
    canvas.Select(look.bold_font);
    row_renderer.DrawTextRow(canvas, rc,
                             gettext(zone_names[unsigned(zone)]));
    return;
  }

  /* the items indented below their header */
  PixelRect item_rc = rc;
  item_rc.left += 2 * Layout::GetTextPadding();

  StaticString<96> name;
  FormatItemName(name, items.items[i]);
  canvas.Select(*look.list.font);
  row_renderer.DrawTextRow(canvas, item_rc, name);
}

void
StatusBarListWidget::OnCursorMoved([[maybe_unused]] unsigned index) noexcept
{
  UpdateButtons();
}

/**
 * Edit the given items in place.
 */
static void
ShowItemsDialog(StatusBarItems &items) noexcept
{
  const StatusBarItems saved = items;

  auto list = std::make_unique<StatusBarListWidget>(items);
  auto &list_ref = *list;
  auto buttons =
    std::make_unique<ButtonPanelWidget>(std::move(list),
                                        ButtonPanelWidget::Alignment::BOTTOM);
  list_ref.SetButtonPanel(*buttons);

  WidgetDialog dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
                      UIGlobals::GetDialogLook(), _("Status bar items"));
  dialog.FinishPreliminary(std::move(buttons));
  dialog.AddButton(_("Close"), mrOK);

  /* Escape puts the items back the way they were */
  if (dialog.ShowModal() != mrOK)
    items = saved;
}

static constexpr StaticEnumChoice style_list[] = {
  { UISettings::StatusBarStyle::WHITE, N_("White"),
    N_("Black text on white.") },
  { UISettings::StatusBarStyle::BLACK, N_("Black"),
    N_("White text on black.") },
  { UISettings::StatusBarStyle::TRANSLUCENT, N_("Translucent"),
    N_("Black text on a translucent white background, through which "
       "the map shows; the map reaches up behind the status bar.") },
  { UISettings::StatusBarStyle::TRANSPARENT, N_("Transparent"),
    N_("Black text on the map, which reaches up behind the status "
       "bar.") },
  nullptr
};

#ifdef HAVE_SYSTEM_STATUS_BAR_SETTING
static constexpr StaticEnumChoice system_status_bar_list[] = {
  { DisplaySettings::SystemStatusBar::AUTO, NC_("Setting", "Auto"),
    N_("Show the system status bar unless the InfoBox area is stretched "
       "to the top screen edge, where it would cover the InfoBoxes.") },
  { DisplaySettings::SystemStatusBar::VISIBLE, N_("Visible"),
    N_("Always show the system status bar, even in full screen mode. "
       "The InfoBoxes cannot be drawn behind it, so they keep clear of "
       "the top screen edge.") },
  { DisplaySettings::SystemStatusBar::HIDDEN, N_("Hidden"),
    N_("Never show the system status bar.") },
  nullptr
};
#endif

class StatusBarSettingsWidget final : public RowFormWidget {
  enum Rows {
#ifdef HAVE_SYSTEM_STATUS_BAR_SETTING
    SYSTEM_STATUS_BAR,
#endif
    SHOW,
    STYLE,
    ITEMS,
  };

  StatusBarItems items;

public:
  StatusBarSettingsWidget() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

void
StatusBarSettingsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                                 [[maybe_unused]] const PixelRect &rc) noexcept
{
  const UISettings &settings = CommonInterface::GetUISettings();
  items = settings.status_bar_items;

#ifdef HAVE_SYSTEM_STATUS_BAR_SETTING
  AddEnum(_("System status bar"),
          _("Whether the system status bar with the clock and the battery "
            "level stays visible."),
          system_status_bar_list,
          unsigned(settings.display.system_status_bar));
#endif

  AddBoolean(_("XCSoar status bar"),
             _("Show a bar at the top of the screen with the page, "
               "values of your choice and the state of GPS, FLARM, "
               "network, logger and battery."),
             settings.show_status_bar);
  AddEnum(_("Style"), nullptr, style_list,
          unsigned(settings.status_bar_style));
  AddButton(_("Items"), [this](){ ShowItemsDialog(items); });
}

bool
StatusBarSettingsWidget::Save(bool &_changed) noexcept
{
  UISettings &settings = CommonInterface::SetUISettings();

#ifdef HAVE_SYSTEM_STATUS_BAR_SETTING
  if (SaveValueEnum(SYSTEM_STATUS_BAR, ProfileKeys::SystemStatusBar,
                    settings.display.system_status_bar)) {
    _changed = true;

    /* this changes the usable screen area */
    CommonInterface::main_window->ApplyFullScreenSettings();
  }
#endif

  bool layout_changed = false;
  layout_changed |= SaveValue(SHOW, ProfileKeys::ShowStatusBar,
                              settings.show_status_bar);
  layout_changed |= SaveValueEnum(STYLE, ProfileKeys::StatusBarStyle,
                                  settings.status_bar_style);

  bool changed = layout_changed;
  if (!(items == settings.status_bar_items)) {
    settings.status_bar_items = items;

    char buffer[1024];
    items.Format(std::span{buffer});
    Profile::map.Set(ProfileKeys::StatusBarItems, buffer);
    changed = true;
  }

  if (layout_changed)
    CommonInterface::main_window->ReinitialiseLayout();

  _changed |= changed;
  return true;
}

bool
ShowStatusBarDialog() noexcept
{
  WidgetDialog dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
                      UIGlobals::GetDialogLook(), _("Status bar"),
                      new StatusBarSettingsWidget());
  dialog.AddButton(_("Close"), mrOK);
  dialog.ShowModal();
  return dialog.GetChanged();
}

const char *
GetStatusBarSummary() noexcept
{
  const UISettings &settings = CommonInterface::GetUISettings();
  if (!settings.show_status_bar) {
#ifdef HAVE_SYSTEM_STATUS_BAR_SETTING
    if (settings.display.system_status_bar !=
        DisplaySettings::SystemStatusBar::HIDDEN)
      return _("System status bar");
#endif
    return _("Off");
  }

  for (const auto &i : style_list)
    if (i.display_string != nullptr &&
        i.id == unsigned(settings.status_bar_style))
      return gettext(i.display_string);

  return _("On");
}
