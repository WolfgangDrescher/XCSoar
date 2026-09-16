// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxArrangeWindow.hpp"
#include "InfoBoxLayout.hpp"
#include "InfoBoxManager.hpp"
#include "Content/Factory.hpp"
#include "Asset.hpp"
#include "Dialogs/HelpDialog.hpp"
#include "Hardware/CPU.hpp"
#include "Language/Language.hpp"
#include "Look/DialogLook.hpp"
#include "Look/InfoBoxLook.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/window/ContainerWindow.hpp"
#include "ui/window/SingleWindow.hpp"
#include "util/StaticString.hxx"
#include "util/StringCompare.hxx"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

#ifdef ENABLE_SDL
#include <SDL_keyboard.h>
#endif

#include <algorithm>
#include <cstdlib>

namespace {

/**
 * How much of the map shines through the overlay?  Only a hint of it
 * is left, so that the text on the backdrop stays legible.
 */
constexpr uint8_t OVERLAY_ALPHA = 0xe6;

/** how long a displaced InfoBox takes to slide into its new slot */
constexpr auto SHUFFLE_DURATION = std::chrono::milliseconds(250);

/** how long an InfoBox has to be held before the picker opens */
constexpr auto PICKER_DELAY = std::chrono::milliseconds(600);

/**
 * How much the card grows when it is picked up, relative to the slot
 * it came from.
 */
[[gnu::pure]]
int
GetDragLift() noexcept
{
  return Layout::Scale(5);
}

/**
 * The distance the finger has to travel before the InfoBox detaches
 * from its slot; below that, the press is still a tap.
 */
[[gnu::pure]]
int
GetDeadZone() noexcept
{
  return Layout::Scale(8);
}

/**
 * Are the InfoBoxes arranged in columns?  This does not follow the
 * screen orientation: several geometries put the InfoBoxes into
 * columns on a portrait screen, and into rows on a landscape one.
 */
[[gnu::pure]]
bool
HasColumns(const InfoBoxLayout::Layout &layout) noexcept
{
  unsigned rows = 0, columns = 0;

  for (unsigned i = 0; i < layout.count; ++i) {
    const PixelPoint p = layout.positions[i].GetCenter();
    bool new_row = true, new_column = true;

    for (unsigned j = 0; j < i; ++j) {
      const PixelPoint q = layout.positions[j].GetCenter();
      new_row &= q.y != p.y;
      new_column &= q.x != p.x;
    }

    rows += new_row;
    columns += new_column;
  }

  /* more rows than columns means the InfoBoxes stand side by side */
  return rows > columns;
}

/**
 * Is a shift key held down?  The window layer passes only the key
 * code, so Shift+Tab has to be recognised here; this follows
 * IsCtrlKeyPressed() in GlueMapWindowEvents.
 */
[[gnu::pure]]
bool
IsShiftKeyPressed() noexcept
{
#ifdef ENABLE_SDL
  return SDL_GetModState() & (KMOD_LSHIFT | KMOD_RSHIFT);
#elif defined(USE_WINUSER)
  return GetKeyState(VK_SHIFT) & 0x8000;
#else
  /* X11 sends XK_ISO_Left_Tab instead; elsewhere Tab only moves
     forwards */
  return false;
#endif
}

} // namespace

bool InfoBoxArrangeWindow::card_floating;

InfoBoxArrangeWindow::InfoBoxArrangeWindow(const InfoBoxLook &_look,
                                           const DialogLook &_dialog_look,
                                           Style _style) noexcept
  :look(_look), dialog_look(_dialog_look), style(_style)
{
  ResetCardNumbers();
}

void
InfoBoxArrangeWindow::Create(ContainerWindow &parent,
                             const PixelRect &rc) noexcept
{
  WindowStyle window_style;
  window_style.Hide();
  window_style.TabStop();
  PaintWindow::Create(parent, rc, window_style);

#ifndef USE_WINUSER
  if (style == Style::MAP)
    /* the map below must still be painted (and invalidated), even
       though this window covers the whole screen */
    SetTransparent();
#endif
}

void
InfoBoxArrangeWindow::SetPanel(InfoBoxSettings::Panel &_panel) noexcept
{
  panel = &_panel;
  ResetCardNumbers();
}

void
InfoBoxArrangeWindow::SetLayout(const InfoBoxLayout::Layout &_layout,
                                PixelRect _content) noexcept
{
  layout = &_layout;
  content = _content;
  columns = HasColumns(_layout);
}

/*
 * geometry
 */

PixelPoint
InfoBoxArrangeWindow::SlotCenter(unsigned slot) const noexcept
{
  return layout->positions[slot].GetCenter();
}

int
InfoBoxArrangeWindow::Along(PixelPoint p) const noexcept
{
  return columns ? p.x : p.y;
}

int
InfoBoxArrangeWindow::Across(PixelPoint p) const noexcept
{
  return columns ? p.y : p.x;
}

int
InfoBoxArrangeWindow::FindSlot(PixelPoint p) const noexcept
{
  for (unsigned i = 0; i < layout->count; ++i)
    if (layout->positions[i].Contains(p))
      return i;

  return -1;
}

PixelRect
InfoBoxArrangeWindow::GetFloatingRect() const noexcept
{
  PixelRect rc = drag->start_rect;
  rc.Offset(drag->pointer.x - drag->origin.x,
            drag->pointer.y - drag->origin.y);

  /* the padding stays behind in the slot; the card itself is picked
     up and grows a little */
  rc.Grow(GetDragLift() - (int)look.preview_padding);

  /* stay inside this window so the card cannot paint over the
     parent's buttons */
  const PixelRect bounds = GetPosition();
  rc.left = std::max(rc.left, bounds.left);
  rc.top = std::max(rc.top, bounds.top);
  rc.right = std::min(rc.right, bounds.right);
  rc.bottom = std::min(rc.bottom, bounds.bottom);
  return rc;
}

double
InfoBoxArrangeWindow::GetSlotFraction(unsigned slot) const noexcept
{
  const PixelPoint center = SlotCenter(slot);

  unsigned count = 0, index = 0;

  for (unsigned i = 0; i < layout->count; ++i) {
    const PixelPoint p = SlotCenter(i);
    if (Along(p) != Along(center))
      /* another row or column */
      continue;

    if (Across(p) < Across(center))
      ++index;

    ++count;
  }

  return (index + 0.5) / count;
}

std::optional<int>
InfoBoxArrangeWindow::FindSlotGroup(int along, int direction) const noexcept
{
  std::optional<int> group;

  for (unsigned i = 0; i < layout->count; ++i) {
    const int g = Along(SlotCenter(i));
    if ((g - along) * direction <= 0)
      continue;

    if (!group || (g - *group) * direction < 0)
      group = g;
  }

  return group;
}

int
InfoBoxArrangeWindow::FindSlotAt(int group, double fraction) const noexcept
{
  int best = -1;
  double best_distance = 0;

  for (unsigned i = 0; i < layout->count; ++i) {
    if (Along(SlotCenter(i)) != group)
      continue;

    const double distance = std::abs(GetSlotFraction(i) - fraction);

    if (best < 0 || distance < best_distance) {
      best = i;
      best_distance = distance;
    }
  }

  return best;
}

PixelRect
InfoBoxArrangeWindow::GetPanelNameRect() const noexcept
{
  PixelRect rc = ToLocal(content);
  rc.Grow(-Layout::Scale(8));
  rc.top = rc.bottom - (int)look.title_font_bold.GetHeight();
  return rc;
}

PixelRect
InfoBoxArrangeWindow::ToLocal(PixelRect rc) const noexcept
{
  const auto origin = GetPosition().GetTopLeft();
  rc.Offset(-origin.x, -origin.y);
  return rc;
}

/*
 * painting
 */

void
InfoBoxArrangeWindow::DrawCard(Canvas &canvas, const PixelRect &rc,
                               unsigned slot, unsigned number,
                               CardState state) noexcept
{
  const int radius = look.preview_radius;
  const bool active = state == CardState::ACTIVE;

  if (state == CardState::FOCUSED) {
    /* filled halo outside the hairline; a thick stroke on this path
       is jagged */
    const int width = look.preview_focus_width;
    PixelRect outer = rc;
    outer.Grow(width);
    const int outer_radius = radius + width;
    canvas.SelectNullPen();
    canvas.Select(Brush{look.preview_active_color});
    canvas.DrawRoundRectangle(outer, {outer_radius * 2, outer_radius * 2});
  }

  canvas.Select(look.preview_border_pen);
  canvas.Select(Brush{active
                      ? look.preview_active_color
                      : look.background_color});
  canvas.DrawRoundRectangle(rc, {radius * 2, radius * 2});

  /* the caption comes from the configuration and not from the InfoBox
     itself, because content providers overwrite the title at runtime
     (the Next Waypoint InfoBox shows the waypoint name, for
     example) */
  const char *caption =
    gettext(InfoBoxFactory::GetCaption(panel->contents[slot]));

  StaticString<8> number_text;
  number_text.Format("#%u", number);

  canvas.SetBackgroundTransparent();
  canvas.SetTextColor(active ? COLOR_WHITE : look.title.fg_color);

  canvas.Select(look.preview_number_font);
  const PixelSize number_size = canvas.CalcTextSize(number_text);

  canvas.Select(look.title_font_bold);
  const PixelSize caption_size = canvas.CalcTextSize(caption);

  /* keep the text off the hairline and the rounded corners */
  PixelRect text_rc = rc;
  text_rc.Grow(-(int)look.preview_padding);
  if (text_rc.GetWidth() <= 0 || text_rc.GetHeight() <= 0)
    text_rc = rc;

  /* both lines are clipped to that inset, so a long caption does not
     run out into the backdrop; if they do not fit at all, they start
     at the top of the inset */
  int y = text_rc.top + std::max(0, ((int)text_rc.GetHeight()
                                     - (int)(number_size.height
                                             + caption_size.height)) / 2);

  canvas.Select(look.preview_number_font);
  canvas.DrawClippedText({std::max(text_rc.left,
                                   text_rc.CenteredTopLeft(number_size).x), y},
                         text_rc, number_text);

  y += number_size.height;

  canvas.Select(look.title_font_bold);
  canvas.DrawClippedText({std::max(text_rc.left,
                                   text_rc.CenteredTopLeft(caption_size).x), y},
                         text_rc, caption);
}

InfoBoxArrangeWindow::CardState
InfoBoxArrangeWindow::GetCardState(unsigned slot) const noexcept
{
  if ((int)slot != described_slot || !HasFocus())
    return CardState::NORMAL;

  return selection == Selection::FOCUSED
    ? CardState::FOCUSED
    : CardState::ACTIVE;
}

void
InfoBoxArrangeWindow::PaintCards(Canvas &canvas) noexcept
{
  const auto paint_slot = [&](unsigned i, CardState state) {
    PixelRect rc = layout->positions[i];
    const PixelPoint offset = GetShuffleOffset(i);
    rc.Offset(offset.x, offset.y);
    rc.Grow(-(int)look.preview_padding);
    DrawCard(canvas, ToLocal(rc), i, card_number[i], state);
  };

  int focused = -1;
  for (unsigned i = 0; i < layout->count; ++i) {
    if (drag && drag->following && i == drag->slot)
      /* this one follows the finger; its slot stays empty */
      continue;

    const CardState state = GetCardState(i);
    if (state == CardState::FOCUSED) {
      focused = (int)i;
      continue;
    }

    paint_slot(i, state);
  }

  /* last, so the outer halo is not covered by a neighbour */
  if (focused >= 0)
    paint_slot(focused, CardState::FOCUSED);

  if (drag && drag->following) {
    const PixelRect rc = GetFloatingRect();
    if (rc.right > rc.left && rc.bottom > rc.top)
      DrawCard(canvas, ToLocal(rc), drag->slot,
               card_number[drag->slot], CardState::ACTIVE);
  }
}

void
InfoBoxArrangeWindow::PaintPanelName(Canvas &canvas) noexcept
{
  if (style != Style::MAP)
    /* the dialog names the panel itself */
    return;

  const char *name = gettext(panel->name);
  if (StringIsEmpty(name))
    return;

  canvas.SetBackgroundTransparent();
  canvas.SetTextColor(look.background_color);
  canvas.Select(look.title_font_bold);

  const PixelRect rc = GetPanelNameRect();
  canvas.TextAutoClipped(rc.CenteredTopLeft(canvas.CalcTextSize(name)), name);
}

void
InfoBoxArrangeWindow::PaintDescription(Canvas &canvas) noexcept
{
  if (described_slot < 0)
    return;

  const auto type = panel->contents[described_slot];
  const char *name = gettext(InfoBoxFactory::GetName(type));
  const char *description = InfoBoxFactory::GetDescription(type);

  PixelRect rc = ToLocal(content);
  rc.Grow(-Layout::Scale(8));

  /* keep clear of the panel name; on a small screen the description
     is cut off instead of covering it */
  if (style == Style::MAP)
    rc.bottom = std::min(rc.bottom, GetPanelNameRect().top - Layout::Scale(8));
  if (rc.bottom <= rc.top)
    return;

  canvas.SetBackgroundTransparent();
  canvas.SetTextColor(style == Style::MAP
                      ? look.background_color
                      : dialog_look.text_color);

  canvas.Select(dialog_look.bold_font);
  name_renderer.Draw(canvas, rc, name);
  rc.top += name_renderer.GetHeight(canvas, rc.GetWidth(), name)
    + Layout::Scale(4);

  if (description != nullptr && rc.top < rc.bottom) {
    const char *text = gettext(description);

    /* the small font is only used when the description does not fit
       into what is left */
    canvas.Select(dialog_look.text_font);
    if (description_renderer.GetHeight(canvas, rc.GetWidth(), text) >
        (unsigned)rc.GetHeight())
      canvas.Select(dialog_look.small_font);

    description_renderer.Draw(canvas, rc, text);
  }
}

void
InfoBoxArrangeWindow::OnPaint(Canvas &canvas) noexcept
{
  if (style == Style::MAP) {
#ifdef ENABLE_OPENGL
    const ScopeAlphaBlend alpha_blend;
    canvas.DrawFilledRectangle(canvas.GetRect(),
                               look.preview_backdrop_color
                               .WithAlpha(OVERLAY_ALPHA));
#else
    /* without alpha blending, hide the map instead of fading it */
    canvas.DrawFilledRectangle(canvas.GetRect(),
                               look.preview_backdrop_color);
#endif
  } else
    canvas.Clear(dialog_look.background_color);

  PaintDescription(canvas);
  PaintPanelName(canvas);

  /* the cards come last: the one which follows the finger must not
     disappear behind anything */
  PaintCards(canvas);
}

/*
 * the InfoBoxes
 */

void
InfoBoxArrangeWindow::Exchange(unsigned a, unsigned b) noexcept
{
  std::swap(panel->contents[a], panel->contents[b]);
  std::swap(card_number[a], card_number[b]);
  OnArrangeModified();
}

void
InfoBoxArrangeWindow::ResetCardNumbers() noexcept
{
  for (unsigned i = 0; i < InfoBoxSettings::Panel::MAX_CONTENTS; ++i)
    card_number[i] = i + 1;
}

void
InfoBoxArrangeWindow::StartShuffle(unsigned slot,
                                   const PixelRect &from) noexcept
{
  if (HasEPaper() || IsSlowCPU())
    /* e-paper and slow CPUs snap instead of animating, the same gate
       the list and the map pan animations use */
    return;

  const PixelRect &to = layout->positions[slot];
  shuffle[slot].offset = {from.left - to.left, from.top - to.top};
  shuffle[slot].start = std::chrono::steady_clock::now();

  shuffle_timer.Schedule(std::chrono::milliseconds(16));
}

PixelPoint
InfoBoxArrangeWindow::GetShuffleOffset(unsigned slot) const noexcept
{
  const auto elapsed = std::chrono::steady_clock::now() - shuffle[slot].start;
  if (elapsed >= SHUFFLE_DURATION)
    return {0, 0};

  const double t = std::chrono::duration<double>(elapsed)
    / std::chrono::duration<double>(SHUFFLE_DURATION);

  /* ease in out cubic, the curve UIKit animates with by default */
  const double e = t < 0.5
    ? 4 * t * t * t
    : 1 - 4 * (1 - t) * (1 - t) * (1 - t);

  return {int(shuffle[slot].offset.x * (1 - e)),
          int(shuffle[slot].offset.y * (1 - e))};
}

void
InfoBoxArrangeWindow::OnShuffleTimer() noexcept
{
  Invalidate();

  for (unsigned i = 0; i < layout->count; ++i) {
    const auto offset = GetShuffleOffset(i);
    if (offset.x != 0 || offset.y != 0)
      return;
  }

  shuffle_timer.Cancel();
}

void
InfoBoxArrangeWindow::OnPickerTimer() noexcept
{
  if (!drag || drag->following)
    return;

  const unsigned slot = drag->slot;
  drag.reset();
  ReleaseCapture();
  Invalidate();

  ShowPicker(slot);
}

void
InfoBoxArrangeWindow::ShowPicker(unsigned slot) noexcept
{
  OnArrangeSuspend();

  if (InfoBoxManager::ShowInfoBoxPicker(*panel, slot)) {
    Invalidate();
    OnArrangeModified();
  }

  OnArrangeActivity();
  SetFocus();
}

void
InfoBoxArrangeWindow::ShowHelp() noexcept
{
  StaticString<768> text;
  text.clear();

  if (HasPointer())
    text = _("Drag an InfoBox onto another one to exchange the two.  "
             "Long press an InfoBox to choose a different InfoBox for "
             "that position.");

  if (HasCursorKeys()) {
    if (!text.empty())
      text.append("\n\n");

    text.append(_("Press Enter to take the selected InfoBox, move it with "
                  "the cursor keys and press Enter again to put it down.  "
                  "Pressing Enter twice without moving an InfoBox chooses "
                  "a different InfoBox for that position."));
  }

  if (extra_help != nullptr) {
    text.append("\n\n");
    text.append(extra_help);
  }

  /* the timeout must not end the mode behind the dialog */
  OnArrangeSuspend();
  HelpDialog(_("Arrange InfoBoxes"), text);
  OnArrangeActivity();
  SetFocus();
}

/*
 * the cursor keys
 */

int
InfoBoxArrangeWindow::FindNeighbour(PixelPoint origin,
                                    int dx, int dy) const noexcept
{
  int best = -1, best_distance = 0;

  for (unsigned i = 0; i < layout->count; ++i) {
    const PixelPoint p = layout->positions[i].GetCenter();
    const int along = (p.x - origin.x) * dx + (p.y - origin.y) * dy;
    if (along <= 0)
      continue;

    /* the closest slot in that direction wins, and among those the
       one which is least off to the side */
    const int across = std::abs((p.x - origin.x) * dy
                                - (p.y - origin.y) * dx);
    const int distance = along + 4 * across;

    if (best < 0 || distance < best_distance) {
      best = i;
      best_distance = distance;
    }
  }

  return best;
}

bool
InfoBoxArrangeWindow::IsAlong(int dx, int dy) const noexcept
{
  return columns ? dx != 0 : dy != 0;
}

int
InfoBoxArrangeWindow::FindNextSlot(int dx, int dy) const noexcept
{
  const PixelPoint origin = SlotCenter(described_slot);

  if (IsAlong(dx, dy)) {
    /* keep the place inside the row (portrait) or column */
    const auto group = FindSlotGroup(Along(origin), dx + dy);
    if (!group)
      return -1;

    return FindSlotAt(*group, cross_fraction);
  }

  return FindNeighbour(origin, dx, dy);
}

void
InfoBoxArrangeWindow::RememberCross() noexcept
{
  if (described_slot >= 0)
    cross_fraction = GetSlotFraction(described_slot);
}

bool
InfoBoxArrangeWindow::MoveFocusToParent(bool forward) noexcept
{
  auto *parent = GetParent();
  if (parent == nullptr)
    return false;

  return forward
    ? parent->FocusNextControl()
    : parent->FocusPreviousControl();
}

bool
InfoBoxArrangeWindow::CanMoveSelection(int dx, int dy) const noexcept
{
  if (selection == Selection::MOVING)
    /* the InfoBox the user carries must not be left behind */
    return true;

  if (described_slot < 0)
    /* the first key press selects the first InfoBox */
    return true;

  if (FindNextSlot(dx, dy) >= 0)
    return true;

  /* Help/Close (map) and the dialog chrome sit next to the cards */
  return GetParent() != nullptr;
}

bool
InfoBoxArrangeWindow::MoveSelection(int dx, int dy) noexcept
{
  OnArrangeActivity();

  if (described_slot < 0) {
    described_slot = 0;
    RememberCross();
    selection = Selection::FOCUSED;
    Invalidate();
    return true;
  }

  if (selection == Selection::MOVING) {
    const int slot = FindNeighbour(SlotCenter(described_slot), dx, dy);
    if (slot < 0)
      return true;

    /* carry the InfoBox along, exchanging it with the neighbour; both
       slide into their new slot */
    Exchange(described_slot, slot);
    StartShuffle(described_slot, layout->positions[slot]);
    StartShuffle(slot, layout->positions[described_slot]);

    described_slot = slot;

    RememberCross();

    Invalidate();
    return true;
  }

  const int slot = FindNextSlot(dx, dy);
  if (slot < 0)
    return MoveFocusToParent(dx + dy > 0);

  described_slot = slot;
  if (!IsAlong(dx, dy))
    RememberCross();

  selection = Selection::FOCUSED;
  Invalidate();
  return true;
}

bool
InfoBoxArrangeWindow::Activate() noexcept
{
  OnArrangeActivity();

  if (described_slot < 0)
    return true;

  if (selection != Selection::MOVING) {
    grab_slot = described_slot;
    selection = Selection::MOVING;
    Invalidate();
    return true;
  }

  const bool unmoved = (unsigned)described_slot == grab_slot;
  selection = Selection::FOCUSED;

  /* the numbers travelled with the InfoBoxes while one was carried;
     now they belong to their slots again */
  ResetCardNumbers();

  Invalidate();

  if (unmoved)
    /* taking and putting down without moving means the user wants to
       change the InfoBox instead */
    ShowPicker(described_slot);

  return true;
}

/*
 * dragging
 */

void
InfoBoxArrangeWindow::FocusSlot(unsigned slot) noexcept
{
  described_slot = slot;
  RememberCross();
  selection = Selection::FOCUSED;
  Invalidate();
}

void
InfoBoxArrangeWindow::BeginDrag(unsigned slot, PixelPoint pointer,
                                bool follow) noexcept
{
  drag.emplace(DragState{slot, layout->positions[slot], pointer, pointer,
                         follow});
  drag_snapshot = *panel;
  ResetCardNumbers();

  /* the card the user has grabbed is the one being described; that
     way only ever one card is highlighted */
  described_slot = slot;
  selection = Selection::TOUCH;

  card_floating = follow;

  if (!follow)
    /* holding the InfoBox still opens the picker */
    picker_timer.Schedule(PICKER_DELAY);

  SetCapture();
  OnArrangeActivity();
  Invalidate();
}

void
InfoBoxArrangeWindow::Drag(PixelPoint p) noexcept
{
  if (!drag)
    return;

  drag->pointer = p;

  if (!drag->following) {
    const auto d = p - drag->origin;
    if (std::abs(d.x) + std::abs(d.y) < GetDeadZone())
      return;

    drag->following = true;
    card_floating = true;
    picker_timer.Cancel();
  }

  /* exchange with the slot the finger has moved into, so that a drag
     across several slots shifts them one by one instead of swapping
     with the slot it started from */
  const int slot = FindSlot(p);
  if (slot >= 0 && unsigned(slot) != drag->slot) {
    Exchange(drag->slot, slot);

    /* the InfoBox which was in the way slides over to the slot the
       dragged one has just left; the dragged one needs no animation
       because it follows the finger */
    StartShuffle(drag->slot, layout->positions[slot]);

    drag->slot = described_slot = slot;
  }

  Invalidate();

  if (style == Style::DIALOG)
    /* the card is not clipped to this window, so the main window has
       to clear what it leaves outside the dialog */
    UIGlobals::GetMainWindow().Invalidate();
}

void
InfoBoxArrangeWindow::Drop() noexcept
{
  if (!drag)
    return;

  picker_timer.Cancel();
  drag.reset();
  card_floating = false;
  ResetCardNumbers();
  RememberCross();
  ReleaseCapture();
  OnArrangeActivity();
  Invalidate();
}

void
InfoBoxArrangeWindow::CancelDrag() noexcept
{
  if (!drag)
    return;

  picker_timer.Cancel();
  drag.reset();
  card_floating = false;
  *panel = drag_snapshot;
  ResetCardNumbers();
  described_slot = -1;

  for (auto &i : shuffle)
    i.start = {};

  OnArrangeModified();
  Invalidate();
}

/*
 * input
 */

bool
InfoBoxArrangeWindow::OnMouseDown(PixelPoint p) noexcept
{
  OnArrangeActivity();

  const PixelPoint parent_p = ToParentCoordinates(p);

  if (const int slot = FindSlot(parent_p); slot >= 0)
    BeginDrag(slot, parent_p, false);

  return true;
}

bool
InfoBoxArrangeWindow::OnMouseMove(PixelPoint p,
                                  [[maybe_unused]] unsigned keys) noexcept
{
  Drag(ToParentCoordinates(p));
  return true;
}

bool
InfoBoxArrangeWindow::OnMouseUp([[maybe_unused]] PixelPoint p) noexcept
{
  if (!drag)
    return true;

  Drop();
  return true;
}

bool
InfoBoxArrangeWindow::OnMouseDouble([[maybe_unused]] PixelPoint p) noexcept
{
  /* arranging InfoBoxes has no use for the menu */
  return true;
}

#ifdef HAVE_MULTI_TOUCH

bool
InfoBoxArrangeWindow::OnMultiTouchDown() noexcept
{
  /* a second finger aborts the drag */
  CancelDrag();
  ReleaseCapture();
  return true;
}

#endif

void
InfoBoxArrangeWindow::OnSetFocus() noexcept
{
  PaintWindow::OnSetFocus();
  Invalidate();
}

void
InfoBoxArrangeWindow::OnKillFocus() noexcept
{
  PaintWindow::OnKillFocus();
  Invalidate();
}

bool
InfoBoxArrangeWindow::OnKeyCheck(unsigned key_code) const noexcept
{
  switch (key_code) {
  case KEY_UP:
    return CanMoveSelection(0, -1);

  case KEY_DOWN:
    return CanMoveSelection(0, 1);

  case KEY_LEFT:
  case KEY_RIGHT:
  case KEY_RETURN:
  case KEY_ESCAPE:
  case KEY_TAB:
#ifdef USE_X11
  case XK_ISO_Left_Tab:
#endif
    return true;

  default:
    return false;
  }
}

bool
InfoBoxArrangeWindow::OnKeyDown(unsigned key_code) noexcept
{
  switch (key_code) {
  case KEY_LEFT:
    return MoveSelection(-1, 0);

  case KEY_RIGHT:
    return MoveSelection(1, 0);

  case KEY_UP:
    return MoveSelection(0, -1);

  case KEY_DOWN:
    return MoveSelection(0, 1);

  case KEY_TAB:
    return MoveFocusToParent(!IsShiftKeyPressed());

#ifdef USE_X11
  case XK_ISO_Left_Tab:
    /* X11 has its own key symbol for Shift+Tab */
    return MoveFocusToParent(false);
#endif

  case KEY_RETURN:
    return Activate();

  case KEY_ESCAPE:
    return OnArrangeCancel();
  }

  return PaintWindow::OnKeyDown(key_code);
}

void
InfoBoxArrangeWindow::OnCancelMode() noexcept
{
  CancelDrag();
  PaintWindow::OnCancelMode();
}
