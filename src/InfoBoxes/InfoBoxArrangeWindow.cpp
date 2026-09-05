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
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Canvas.hpp"
#include "util/StaticString.hxx"
#include "util/StringCompare.hxx"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
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

} // namespace

InfoBoxArrangeWindow::InfoBoxArrangeWindow(const InfoBoxLook &_look,
                                           const DialogLook &_dialog_look)
  noexcept
  :look(_look), dialog_look(_dialog_look),
   button_renderer(_dialog_look.button)
{
  ResetCardNumbers();
}

void
InfoBoxArrangeWindow::AddButton(const char *caption,
                                Callback callback) noexcept
{
  auto &button = buttons.append();
  button.caption = caption;
  button.callback = std::move(callback);
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
}

/*
 * geometry
 */

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
  return rc;
}

int
InfoBoxArrangeWindow::GetButtonGap() noexcept
{
  return Layout::Scale(8);
}

int
InfoBoxArrangeWindow::GetButtonWidth() const noexcept
{
  const int count = buttons.size();
  const int gap = GetButtonGap();

  /* wide enough for the longest caption, so that no button has to
     wrap its text */
  int width = Layout::Scale(80);
  for (const auto &i : buttons)
    width = std::max(width,
                     (int)TextButtonRenderer::
                     GetMinimumButtonWidth(dialog_look.button, i.caption));

  /* but never wider than the row itself */
  return std::min(width,
                  ((int)content.GetWidth() - (count + 1) * gap) / count);
}

PixelRect
InfoBoxArrangeWindow::GetButtonRowRect() const noexcept
{
  const int count = buttons.size();
  const int height = (int)Layout::GetMaximumControlHeight();
  const int gap = GetButtonGap();
  const int total = count * GetButtonWidth() + (count - 1) * gap;

  PixelRect r;
  r.bottom = content.bottom - gap;
  r.top = r.bottom - height;
  r.left = (content.left + content.right - total) / 2;
  r.right = r.left + total;
  return r;
}

PixelRect
InfoBoxArrangeWindow::GetButtonRect(int i) const noexcept
{
  const int width = GetButtonWidth();

  PixelRect r = GetButtonRowRect();
  r.left += i * (width + GetButtonGap());
  r.right = r.left + width;
  return r;
}

int
InfoBoxArrangeWindow::FindButton(PixelPoint p) const noexcept
{
  for (unsigned i = 0; i < buttons.size(); ++i)
    if (GetButtonRect(i).Contains(p))
      return i;

  return -1;
}

PixelRect
InfoBoxArrangeWindow::GetPanelNameRect() const noexcept
{
  PixelRect rc = ToLocal(GetButtonRowRect());
  rc.bottom = rc.top - Layout::Scale(12);
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
                               bool active) noexcept
{
  const int radius = look.preview_radius;

  canvas.SelectNullPen();
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

  /* both lines are clipped to the card, so that a caption which is
     too long for it does not run out into the backdrop; if they do
     not fit at all, they start at the top edge */
  int y = rc.top + std::max(0, ((int)rc.GetHeight()
                                - (int)(number_size.height
                                        + caption_size.height)) / 2);

  canvas.Select(look.preview_number_font);
  canvas.DrawClippedText({std::max(rc.left,
                                   rc.CenteredTopLeft(number_size).x), y},
                         rc, number_text);

  y += number_size.height;

  canvas.Select(look.title_font_bold);
  canvas.DrawClippedText({std::max(rc.left,
                                   rc.CenteredTopLeft(caption_size).x), y},
                         rc, caption);
}

void
InfoBoxArrangeWindow::PaintCards(Canvas &canvas) noexcept
{
  for (unsigned i = 0; i < layout->count; ++i) {
    if (drag && drag->following && i == drag->slot)
      /* this one follows the finger; its slot stays empty */
      continue;

    PixelRect rc = layout->positions[i];
    const PixelPoint offset = GetShuffleOffset(i);
    rc.Offset(offset.x, offset.y);
    rc.Grow(-(int)look.preview_padding);
    DrawCard(canvas, ToLocal(rc), i, card_number[i], false);
  }

  if (drag && drag->following)
    DrawCard(canvas, ToLocal(GetFloatingRect()), drag->slot,
             card_number[drag->slot], true);
}

void
InfoBoxArrangeWindow::PaintButtons(Canvas &canvas) noexcept
{
  for (unsigned i = 0; i < buttons.size(); ++i) {
    button_renderer.SetCaption(buttons[i].caption);
    button_renderer.DrawButton(canvas, ToLocal(GetButtonRect(i)),
                               GetButtonState(i));
  }
}

void
InfoBoxArrangeWindow::PaintPanelName(Canvas &canvas) noexcept
{
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
InfoBoxArrangeWindow::OnPaint(Canvas &canvas) noexcept
{
#ifdef ENABLE_OPENGL
  {
    const ScopeAlphaBlend alpha_blend;
    canvas.DrawFilledRectangle(canvas.GetRect(),
                               look.preview_backdrop_color
                               .WithAlpha(OVERLAY_ALPHA));
  }
#else
  /* without alpha blending, hide the map instead of fading it */
  canvas.DrawFilledRectangle(canvas.GetRect(),
                             look.preview_backdrop_color);
#endif

  PaintPanelName(canvas);
  PaintButtons(canvas);

  /* the cards come last: the one which follows the finger must not
     disappear behind anything */
  PaintCards(canvas);
}

/*
 * the InfoBoxes
 */

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
InfoBoxArrangeWindow::ShowPicker(unsigned slot) noexcept
{
  OnArrangeSuspend();

  if (InfoBoxManager::ShowInfoBoxPicker(*panel, slot))
    Invalidate();

  OnArrangeActivity();
  SetFocus();
}

void
InfoBoxArrangeWindow::ShowHelp() noexcept
{
  /* the timeout must not end the mode behind the dialog */
  OnArrangeSuspend();
  HelpDialog(_("Arrange InfoBoxes"),
             _("Drag an InfoBox onto another one to exchange the two."));
  OnArrangeActivity();
  SetFocus();
}

/*
 * the buttons
 */

ButtonState
InfoBoxArrangeWindow::GetButtonState(int i) const noexcept
{
  return held_button == i && button_down
    ? ButtonState::PRESSED
    : ButtonState::ENABLED;
}

void
InfoBoxArrangeWindow::OnButton(int i) noexcept
{
  buttons[i].callback();
}

void
InfoBoxArrangeWindow::HoldButton(int i, bool down) noexcept
{
  if (i == held_button && down == button_down)
    return;

  held_button = i;
  button_down = down;
  Invalidate();
}

void
InfoBoxArrangeWindow::ReleaseButton() noexcept
{
  if (held_button < 0)
    return;

  held_button = -1;
  button_down = false;
  Invalidate();
}

/*
 * dragging
 */

void
InfoBoxArrangeWindow::BeginDrag(unsigned slot, PixelPoint pointer,
                                bool follow) noexcept
{
  drag.emplace(DragState{slot, layout->positions[slot], pointer, pointer,
                         follow});
  drag_snapshot = *panel;
  ResetCardNumbers();

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
  }

  /* exchange with the slot the finger has moved into, so that a drag
     across several slots shifts them one by one instead of swapping
     with the slot it started from */
  const int slot = FindSlot(p);
  if (slot >= 0 && unsigned(slot) != drag->slot) {
    std::swap(panel->contents[drag->slot], panel->contents[slot]);
    std::swap(card_number[drag->slot], card_number[slot]);

    /* the InfoBox which was in the way slides over to the slot the
       dragged one has just left; the dragged one needs no animation
       because it follows the finger */
    StartShuffle(drag->slot, layout->positions[slot]);

    drag->slot = slot;
  }

  Invalidate();
}

void
InfoBoxArrangeWindow::Drop() noexcept
{
  if (!drag)
    return;

  drag.reset();
  ResetCardNumbers();
  ReleaseCapture();
  OnArrangeActivity();
  Invalidate();
}

void
InfoBoxArrangeWindow::CancelDrag() noexcept
{
  if (!drag)
    return;

  drag.reset();
  *panel = drag_snapshot;
  ResetCardNumbers();

  for (auto &i : shuffle)
    i.start = {};

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

  if (const int button = FindButton(parent_p); button >= 0)
    HoldButton(button, true);
  else if (const int slot = FindSlot(parent_p); slot >= 0)
    BeginDrag(slot, parent_p, false);

  return true;
}

bool
InfoBoxArrangeWindow::OnMouseMove(PixelPoint p,
                                  [[maybe_unused]] unsigned keys) noexcept
{
  const PixelPoint parent_p = ToParentCoordinates(p);

  if (held_button >= 0)
    HoldButton(held_button, GetButtonRect(held_button).Contains(parent_p));
  else
    Drag(parent_p);

  return true;
}

bool
InfoBoxArrangeWindow::OnMouseUp([[maybe_unused]] PixelPoint p) noexcept
{
  if (held_button >= 0) {
    const int button = held_button;
    const bool clicked = button_down;
    ReleaseButton();

    if (clicked)
      OnButton(button);

    return true;
  }

  if (!drag)
    return true;

  const unsigned slot = drag->slot;
  const bool tap = !drag->following;
  Drop();

  if (tap)
    /* a tap: let the user choose the contents of this InfoBox */
    ShowPicker(slot);

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
InfoBoxArrangeWindow::OnCancelMode() noexcept
{
  ReleaseButton();
  CancelDrag();
  PaintWindow::OnCancelMode();
}
