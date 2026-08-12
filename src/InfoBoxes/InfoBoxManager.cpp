// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/InfoBoxManager.hpp"
#include "InfoBoxes/InfoBoxWindow.hpp"
#include "InfoBoxes/InfoBoxLayout.hpp"
#include "InfoBoxes/Content/Factory.hpp"
#include "Language/Language.hpp"
#include "Form/DataField/ComboList.hpp"
#include "Dialogs/ComboPicker.hpp"
#include "Profile/InfoBoxConfig.hpp"
#include "Profile/Current.hpp"
#include "Interface.hpp"
#include "MainWindow.hpp"
#include "UIState.hpp"

#include <algorithm>
#include <cstdint>

namespace InfoBoxManager {

InfoBoxLayout::Layout layout;

/**
 * The layout as calculated from the geometry alone, before the
 * contents of the current panel were applied to it.
 */
static InfoBoxLayout::Layout base_layout;

/**
 * The panel contents which #layout was derived from.
 */
static InfoBoxFactory::Type
layout_contents[InfoBoxSettings::Panel::MAX_CONTENTS];

/**
 * Is this the initial DisplayInfoBox() call?  If yes, then all
 * content objects need to be created.
 */
static bool first;

static void
DisplayInfoBox() noexcept;

static void
InfoBoxDrawIfDirty() noexcept;

static void
UpdateLayout(const InfoBoxSettings::Panel &panel) noexcept;

} // namespace InfoBoxManager

static bool infoboxes_dirty = false;
static bool infoboxes_hidden = false;

/**
 * Bit mask of the InfoBox slots which are currently configured as
 * #InfoBoxFactory::e_Invisible.  Kept up to date by DisplayInfoBox();
 * a change of this mask means the map window needs to be resized.
 */
static uint_least32_t invisible_mask = 0;

static_assert(InfoBoxSettings::Panel::MAX_CONTENTS <= 32,
              "invisible_mask is too small");

/* True after Create() finishes and until Destroy() runs.  Startup can
   re-enter layout (terrain load, PageActions::Update) while windows
   are half-built; defer refresh via ScheduleRefreshInfoBoxes() and
   skip work here until the manager is ready. */
static bool infoboxes_ready = false;

static InfoBoxWindow *infoboxes[InfoBoxSettings::Panel::MAX_CONTENTS];

[[gnu::pure]]
static const InfoBoxSettings::Panel &
GetCurrentPanel() noexcept
{
  const unsigned panel = CommonInterface::GetUIState().panel_index;
  return CommonInterface::GetUISettings().info_boxes.panels[panel];
}

/**
 * Is the given slot of the current panel configured as
 * #InfoBoxFactory::e_Invisible?  Such an InfoBox is never shown; the
 * map window is extended over it instead.
 */
[[gnu::pure]]
static bool
IsInvisible(const InfoBoxSettings::Panel &panel, unsigned i) noexcept
{
  return panel.contents[i] == InfoBoxFactory::e_Invisible;
}

/**
 * Recalculate #invisible_mask; returns true if it has changed, which
 * means the map window needs to be moved.
 */
static bool
UpdateInvisibleMask() noexcept
{
  const InfoBoxSettings::Panel &panel = GetCurrentPanel();

  uint_least32_t mask = 0;
  for (unsigned i = 0; i < InfoBoxManager::layout.count; ++i)
    if (IsInvisible(panel, i))
      mask |= uint_least32_t(1) << i;

  if (mask == invisible_mask)
    return false;

  invisible_mask = mask;
  return true;
}

PixelRect
InfoBoxManager::ExpandOverInvisible(PixelRect rc) noexcept
{
  if (invisible_mask == 0)
    return rc;

  for (unsigned i = 0; i < layout.count; ++i) {
    if ((invisible_mask & (uint_least32_t(1) << i)) == 0)
      continue;

    const PixelRect &ib = layout.positions[i];
    rc.left = std::min(rc.left, ib.left);
    rc.top = std::min(rc.top, ib.top);
    rc.right = std::max(rc.right, ib.right);
    rc.bottom = std::max(rc.bottom, ib.bottom);
  }

  return rc;
}

// TODO locking
void
InfoBoxManager::Hide() noexcept
{
  if (infoboxes_hidden)
    return;

  infoboxes_hidden = true;

  if (!infoboxes_ready)
    return;

  for (unsigned i = 0; i < layout.count; i++) {
    if (infoboxes[i] != nullptr)
      infoboxes[i]->FastHide();
  }
}

void
InfoBoxManager::Show() noexcept
{
  if (!infoboxes_hidden)
    return;

  infoboxes_hidden = false;

  if (!infoboxes_ready)
    return;

  const InfoBoxSettings::Panel &panel = GetCurrentPanel();

  for (unsigned i = 0; i < layout.count; i++) {
    /* "invisible" InfoBoxes stay hidden (the map is drawn there), and
       so do those which have released their space */
    if (infoboxes[i] != nullptr && layout.visible[i] &&
        !IsInvisible(panel, i))
      infoboxes[i]->Show();
  }

  SetDirty();
}

void
InfoBoxManager::UpdateLayout(const InfoBoxSettings::Panel &panel) noexcept
{
  if (std::equal(layout_contents, layout_contents + layout.count,
                 panel.contents))
    return;

  std::copy_n(panel.contents, layout.count, layout_contents);

  layout = base_layout;
  InfoBoxLayout::ApplyContents(layout, panel);

  const auto border_style =
    CommonInterface::GetUISettings().info_boxes.border_style;

  for (unsigned i = 0; i < layout.count; ++i) {
    if (infoboxes[i] == nullptr)
      continue;

    if (!layout.visible[i]) {
      infoboxes[i]->Hide();
      continue;
    }

    infoboxes[i]->SetBorderKind(border_style == InfoBoxSettings::BorderStyle::TAB
                                ? 0
                                : layout.borders[i]);
    infoboxes[i]->Move(layout.positions[i]);

    if (!infoboxes_hidden && !IsInvisible(panel, i))
      infoboxes[i]->Show();
  }
}

void
InfoBoxManager::DisplayInfoBox() noexcept
{
  static int DisplayTypeLast[InfoBoxSettings::Panel::MAX_CONTENTS];
  static bool displaying = false;

  if (!infoboxes_ready || displaying)
    return;

  displaying = true;

  // JMW note: this is updated every GPS time step

  const unsigned panel = CommonInterface::GetUIState().panel_index;

  const InfoBoxSettings::Panel &settings =
    CommonInterface::GetUISettings().info_boxes.panels[panel];

  UpdateLayout(settings);

  for (unsigned i = 0; i < layout.count; i++) {
    if (infoboxes[i] == nullptr)
      continue;

    // All calculations are made in a separate thread. Slow calculations
    // should apply to the function DoCalculationsSlow()
    // Do not put calculations here!

    InfoBoxFactory::Type DisplayType = settings.contents[i];
    if ((unsigned)DisplayType > (unsigned)InfoBoxFactory::MAX_TYPE_VAL)
      DisplayType = InfoBoxFactory::NavAltitude;

    const bool needupdate = ((DisplayType != DisplayTypeLast[i]) || first);

    if (needupdate || !infoboxes[i]->HasContent()) {
      infoboxes[i]->SetTitle(gettext(InfoBoxFactory::GetCaption(DisplayType)));
      infoboxes[i]->SetContentProvider(InfoBoxFactory::Create(DisplayType));
      DisplayTypeLast[i] = DisplayType;

      if (DisplayType == InfoBoxFactory::e_Invisible || !layout.visible[i])
        infoboxes[i]->FastHide();
      else if (!infoboxes_hidden)
        infoboxes[i]->Show();
    }

    infoboxes[i]->UpdateContent();
  }

  first = false;
  displaying = false;

  if (UpdateInvisibleMask() && CommonInterface::main_window != nullptr)
    /* the set of "invisible" InfoBoxes has changed: grow or shrink
       the map window accordingly */
    CommonInterface::main_window->RelayoutMapArea();
}

void
InfoBoxManager::InfoBoxDrawIfDirty() noexcept
{
  // No need to redraw map or infoboxes if screen is blanked.
  // This should save lots of battery power due to CPU usage
  // of drawing the screen

  if (infoboxes_dirty && !infoboxes_hidden &&
      !CommonInterface::GetUIState().screen_blanked) {
    DisplayInfoBox();
    infoboxes_dirty = false;
  }
}

InfoBoxWindow *
InfoBoxManager::GetWindow(unsigned id) noexcept
{
  if (!infoboxes_ready || id >= layout.count)
    return nullptr;

  return infoboxes[id];
}

void
InfoBoxManager::SetDirty() noexcept
{
  infoboxes_dirty = true;
}

void
InfoBoxManager::InvalidateAfterLanguageChange() noexcept
{
  first = true;
  SetDirty();
}

void
InfoBoxManager::ScheduleRedraw() noexcept
{
  if (infoboxes_hidden || !infoboxes_ready)
    return;

  for (unsigned i = 0; i < layout.count; i++) {
    if (infoboxes[i] != nullptr)
      infoboxes[i]->Invalidate();
  }
}

bool
InfoBoxManager::IsReady() noexcept
{
  return infoboxes_ready;
}

void
InfoBoxManager::ProcessTimer() noexcept
{
  if (!infoboxes_ready)
    return;

  InfoBoxDrawIfDirty();
}

void
InfoBoxManager::Create(ContainerWindow &parent,
                       const InfoBoxLayout::Layout &_layout,
                       const InfoBoxLook &look) noexcept
{
  const InfoBoxSettings &settings =
    CommonInterface::GetUISettings().info_boxes;

  const InfoBoxSettings::Panel &panel =
    settings.panels[CommonInterface::GetUIState().panel_index];

  infoboxes_ready = false;
  first = true;
  base_layout = _layout;
  layout = _layout;
  InfoBoxLayout::ApplyContents(layout, panel);
  std::copy_n(panel.contents, layout.count, layout_contents);

  /* determine the "invisible" slots before the caller queries
     ExpandOverInvisible() to position the map window */
  invisible_mask = 0;
  UpdateInvisibleMask();

  for (unsigned i = layout.count; i < InfoBoxSettings::Panel::MAX_CONTENTS; ++i)
    infoboxes[i] = nullptr;

  WindowStyle style;
  style.Hide();

  // create infobox windows
  for (unsigned i = layout.count; i-- > 0;) {
    const PixelRect &rc = layout.positions[i];
    int Border =
      settings.border_style == InfoBoxSettings::BorderStyle::TAB
      ? 0
      /* layout.borders is derived from the effective layout, while
         settings.geometry is the configured layout */
      : layout.borders[i];

    infoboxes[i] = new InfoBoxWindow(parent, rc,
                                     Border, settings, look,
                                     i, style);
  }

  infoboxes_hidden = true;
  infoboxes_ready = true;
}

void
InfoBoxManager::Destroy() noexcept
{
  infoboxes_ready = false;
  first = true;

  for (unsigned i = 0; i < InfoBoxSettings::Panel::MAX_CONTENTS; ++i) {
    delete infoboxes[i];
    infoboxes[i] = nullptr;
  }
}

void
InfoBoxManager::ShowInfoBoxPicker(const int i) noexcept
{
  InfoBoxSettings &settings = CommonInterface::SetUISettings().info_boxes;
  const unsigned panel_index = CommonInterface::GetUIState().panel_index;
  InfoBoxSettings::Panel &panel = settings.panels[panel_index];

  const InfoBoxFactory::Type old_type = panel.contents[i];

  ComboList list;
  for (unsigned j = InfoBoxFactory::MIN_TYPE_VAL; j < InfoBoxFactory::NUM_TYPES; j++) {
    const char *desc = InfoBoxFactory::GetDescription((InfoBoxFactory::Type)j);
    list.Append(j, gettext(InfoBoxFactory::GetName((InfoBoxFactory::Type)j)),
                gettext(InfoBoxFactory::GetName((InfoBoxFactory::Type)j)),
                desc != NULL ? gettext(desc) : NULL);
  }

  list.Sort();
  list.current_index = list.LookUp(old_type);

  /* let the user select */

  StaticString<20> caption;
  caption.Format("%s: %d", _("InfoBox"), i + 1);
  int result = ComboPicker(caption, list, nullptr, true);
  if (result < 0)
    return;

  /* was there a modification? */

  InfoBoxFactory::Type new_type = (InfoBoxFactory::Type)list[result].int_value;
  if (new_type == old_type)
    return;

  /* yes: apply and save it */

  panel.contents[i] = new_type;
  DisplayInfoBox();

  Profile::Save(Profile::map, panel, panel_index);
}

void
InfoBoxManager::ClearFocusExcept(unsigned except_id) noexcept
{
  for (unsigned i = 0; i < layout.count; i++) {
    if (i != except_id && infoboxes[i] != nullptr && infoboxes[i]->HasFocus()) {
      infoboxes[i]->FocusParent();
    }
  }
}
