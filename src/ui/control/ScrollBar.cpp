// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ScrollBar.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "ui/window/PaintWindow.hpp"
#include "Asset.hpp"
#include "Hardware/CPU.hpp"
#include "Look/ButtonLook.hpp"
#include "util/Compiler.h"
#include "util/Macros.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

#include <cassert>
#include <chrono>

/**
 * How long the #ScrollBar::Style::WHEN_SCROLLING indicator stays
 * fully visible after the last scroll movement.
 */
static constexpr auto INDICATOR_HOLD = std::chrono::milliseconds{800};

/**
 * How long the indicator stays visible while the pointer rests on it.
 * A hover is refreshed by every mouse move; this is the safety net
 * for the pointer leaving the window, where no more moves arrive.
 */
static constexpr auto INDICATOR_HOVER_HOLD = std::chrono::seconds{3};

/** The interval between two fade-out steps. */
static constexpr auto INDICATOR_FADE_INTERVAL = std::chrono::milliseconds{40};

/** The opacity of the indicator while it is fully visible. */
static constexpr uint8_t INDICATOR_ALPHA = 0xcc;

/** The opacity of the track behind the indicator, and its two grays. */
static constexpr uint8_t INDICATOR_TRACK_ALPHA = 0xf4;
static constexpr uint8_t INDICATOR_TRACK_GRAY = 0xf4;
static constexpr uint8_t INDICATOR_TRACK_BORDER_GRAY = 0xd2;

/** How much opacity the indicator loses per fade-out step. */
static constexpr uint8_t INDICATOR_FADE_STEP = 0x20;

/**
 * The width of the #ScrollBar::Style::WHEN_SCROLLING indicator, and
 * its width while the pointer rests on it or drags it.  These are the
 * measurements of the overlay scroll bars on macOS.
 */
static constexpr unsigned INDICATOR_WIDTH_PT = 6;
static constexpr unsigned INDICATOR_WIDE_WIDTH_PT = 11;

/**
 * The gap between the indicator and the right edge of the window, and
 * the one above and below it, so that it does not touch the edges.
 */
static constexpr unsigned INDICATOR_MARGIN_PT = 2;
static constexpr unsigned INDICATOR_PADDING_PT = 3;

/** The hairline between the track and the content. */
static constexpr unsigned INDICATOR_BORDER_PT = 1;

/**
 * The global scroll bar style; see ScrollBar::SetGlobalStyle().  It
 * defaults to the traditional scroll bar, so that unconfigured code
 * paths (e.g. test programs) behave as before.
 */
static ScrollBar::Style global_style = ScrollBar::Style::ALWAYS;

void
ScrollBar::SetGlobalStyle(Style _style) noexcept
{
  global_style = _style;
}

ScrollBar::Style
ScrollBar::GetGlobalStyle() noexcept
{
  return global_style;
}

unsigned
ScrollBar::GetBarWidth() noexcept
{
  // if the device has a pointer (mouse/touchscreen/etc.)
  if (HasPointer())
    /* with a mouse, the scroll bar can be smaller */
    return Layout::GetMinimumControlHeight();
  else
    // thin for devices without touch screen
    return Layout::VptScale(10);
}

unsigned
ScrollBar::GetScrollStep() noexcept
{
  return GetBarWidth();
}

/**
 * Can the indicator be faded out smoothly?  E-paper and slow CPUs
 * cannot keep up with the animation and hide it in one step instead
 * (same gate as the scroll animations in List and VScrollPanel).
 */
[[gnu::pure]]
static bool
UseIndicatorFade() noexcept
{
  return !HasEPaper() && !IsSlowCPU();
}

ScrollBar::ScrollBar(PaintWindow &_window,
                     const ButtonLook &_button_look) noexcept
  :window(_window), button_renderer(_button_look), dragging(false)
{
  // Reset the ScrollBar on creation
  Reset();
}

ScrollBar::~ScrollBar() noexcept = default;

void
ScrollBar::SetSize(const PixelSize size) noexcept
{
  style = GetGlobalStyle();

  unsigned width, margin, padding;

  if (style == Style::WHEN_SCROLLING) {
    /* a thin bar, inset from the edges, like the overlay scroll bars
       on macOS */
    width = std::max(Layout::VptScale(INDICATOR_WIDTH_PT), 3u);
    margin = std::max(Layout::VptScale(INDICATOR_MARGIN_PT), 2u);
    padding = std::max(Layout::VptScale(INDICATOR_PADDING_PT), 2u);
  } else {
    width = GetBarWidth();
    margin = 0;
    padding = 0;
  }

  indicator_margin = margin;
  indicator_padding = padding;

  // Update the coordinates of the scrollbar
  rc.left = size.width - width - margin;
  rc.top = padding;
  rc.right = size.width - margin;
  rc.bottom = size.height - padding;
}

void
ScrollBar::Reset() noexcept
{
  rc.SetEmpty();
  rc_slider.SetEmpty();
  HideIndicator();
}

/** The width of the indicator while it is hovered or dragged */
[[gnu::pure]]
static unsigned
GetWideIndicatorWidth() noexcept
{
  return std::max(Layout::VptScale(INDICATOR_WIDE_WIDTH_PT), 5u);
}

/** The gap between the indicator and the edges of its track */
[[gnu::pure]]
static unsigned
GetIndicatorTrackInset() noexcept
{
  return std::max(Layout::VptScale(INDICATOR_BORDER_PT), 1u);
}

PixelRect
ScrollBar::GetIndicatorTrack() const noexcept
{
  const int inset = (int)GetIndicatorTrackInset();
  const int right = rc.right + (int)indicator_margin;

  /* the track holds the wide indicator, one inset away from its right
     edge and two from its left one */
  return PixelRect{
    PixelPoint{right - (int)GetWideIndicatorWidth() - 3 * inset,
               rc.top - (int)indicator_padding},
    PixelPoint{right, rc.bottom + (int)indicator_padding},
  };
}

PixelRect
ScrollBar::GetIndicatorGrabRect() const noexcept
{
  PixelRect r = GetIndicatorTrack();

  /* wider than the bar, so that a finger can hit it */
  r.left = std::min(r.left,
                    r.right - (int)std::max(Layout::GetMinimumControlHeight() / 2,
                                            (unsigned)r.GetWidth() * 2));

  /* and with some slack past the ends of the thumb, because the thumb
     of a long list is short */
  const int slack = (int)Layout::VptScale(6);
  r.top = std::max(r.top, rc_slider.top - slack);
  r.bottom = std::min(r.bottom, rc_slider.bottom + slack);

  return r;
}

PixelRect
ScrollBar::GetIndicatorRect() const noexcept
{
  PixelRect r = rc_slider;

  if (IsIndicatorWide()) {
    /* grow into the margin and to the left, so that the bar under the
       pointer looks like a scroll bar that can be grabbed */
    const int inset = (int)GetIndicatorTrackInset();
    r.right = rc.right + (int)indicator_margin - inset;
    r.left = r.right - (int)GetWideIndicatorWidth();
  }

  return r;
}

void
ScrollBar::InvalidateIndicator() noexcept
{
  if (window.IsDefined())
    window.Invalidate(GetIndicatorTrack());
}

bool
ScrollBar::IsInsideSlider(PixelPoint pt) const noexcept
{
  if (IsReservingSpace())
    return rc_slider.Contains(pt);

  /* the overlay indicator can only be grabbed while it is visible */
  if (indicator_alpha == 0 || rc_slider.GetHeight() <= 0)
    return false;

  return GetIndicatorGrabRect().Contains(pt);
}

void
ScrollBar::NotifyMouseMove(PixelPoint pt) noexcept
{
  if (style != Style::WHEN_SCROLLING || !IsDefined() || dragging)
    return;

  const bool hover = GetIndicatorTrack().Contains(pt);
  const bool changed = hover != indicator_hover;

  indicator_hover = hover;

  if (hover) {
    /* show it right away and hold it while the pointer rests on it */
    const bool appeared = indicator_alpha != INDICATOR_ALPHA || !indicator_wide;
    indicator_alpha = INDICATOR_ALPHA;
    indicator_wide = true;
    indicator_timer.Schedule(INDICATOR_HOVER_HOLD);

    if (appeared)
      InvalidateIndicator();
  } else if (changed)
    /* fade it out like after a scroll movement; it keeps its wide
       shape until it is gone */
    indicator_timer.Schedule(INDICATOR_HOLD);
}

void
ScrollBar::NotifyScroll() noexcept
{
  if (style != Style::WHEN_SCROLLING || !IsDefined())
    return;

  const bool was_invisible = indicator_alpha == 0;
  indicator_alpha = INDICATOR_ALPHA;

  if (!indicator_hover)
    indicator_timer.Schedule(INDICATOR_HOLD);

  if (was_invisible)
    InvalidateIndicator();
}

void
ScrollBar::HideIndicator() noexcept
{
  indicator_timer.Cancel();
  indicator_alpha = 0;
  indicator_hover = false;
  indicator_wide = false;
}

void
ScrollBar::OnIndicatorTimer() noexcept
{
  if (indicator_alpha == 0) {
    indicator_timer.Cancel();
    return;
  }

  if (UseIndicatorFade() && indicator_alpha > INDICATOR_FADE_STEP) {
    indicator_alpha -= INDICATOR_FADE_STEP;
    indicator_timer.Schedule(INDICATOR_FADE_INTERVAL);
  } else {
    indicator_alpha = 0;
    indicator_hover = false;
    indicator_wide = false;
    indicator_timer.Cancel();
  }

  InvalidateIndicator();
}

void
ScrollBar::SetSlider(unsigned size, unsigned view_size,
                     unsigned origin) noexcept
{
  const int netto_height = GetNettoHeight();

  // If (no size) slider fills the whole area (no scrolling)
  int height = size > 0
    ? (int)(netto_height * view_size / size)
    : netto_height;
  // Prevent the slider from getting to small
  const int min_height = style == Style::WHEN_SCROLLING
    /* the indicator is only a few pixels wide, but it must stay long
       enough to be recognisable */
    ? (int)Layout::VptScale(16)
    : GetWidth();
  if (height < min_height)
    height = min_height;

  if (height > netto_height)
    height = netto_height;

  // Calculate highest origin (counted in ListItems)
  unsigned max_origin = size - view_size;

  // Move the slider to the appropriate position
  int top = (max_origin > 0) ?
      ((netto_height - height) * origin / max_origin) : 0;

  // Prevent the slider from getting to big
  // TODO: not needed?!
  if (top + height > netto_height)
    height = netto_height - top;

  // Update slider coordinates
  rc_slider.left = rc.left;
  rc_slider.top = rc.top + GetArrowHeight() + top;
  rc_slider.right = rc.right;
  rc_slider.bottom = rc_slider.top + height;
}

unsigned
ScrollBar::ToOrigin(unsigned size, unsigned view_size, int y) const noexcept
{
  // Calculate highest origin (counted in ListItems)
  unsigned max_origin = size - view_size;
  if (max_origin <= 0)
    return 0;

  y -= rc.top + GetArrowHeight();
  if (y < 0)
    return 0;

  unsigned origin = y * max_origin / GetScrollHeight();
  return std::min(origin, max_origin);
}

void
ScrollBar::Paint(Canvas &canvas) noexcept
{
  Paint(canvas, ButtonState::ENABLED, ButtonState::ENABLED);
}

void
ScrollBar::Paint(Canvas &canvas, ButtonState up_state,
                 ButtonState down_state) noexcept
{
  if (style == Style::WHEN_SCROLLING)
    PaintIndicator(canvas);
  else
    PaintBar(canvas, up_state, down_state);
}

void
ScrollBar::PaintIndicator(Canvas &canvas) noexcept
{
  if (indicator_alpha == 0 || rc_slider.GetHeight() <= 0)
    return;

  const PixelRect rc_thumb = GetIndicatorRect();

#ifdef ENABLE_OPENGL
  const ScopeAlphaBlend alpha_blend;
  const bool wide = IsIndicatorWide();

  if (wide) {
    /* the track shows up together with the wide bar, so that it reads
       as something that can be grabbed */
    const PixelRect track = GetIndicatorTrack();
    const uint8_t track_alpha = (uint8_t)(indicator_alpha
                                          * INDICATOR_TRACK_ALPHA
                                          / INDICATOR_ALPHA);
    canvas.DrawFilledRectangle(track,
                               Color(INDICATOR_TRACK_GRAY,
                                     INDICATOR_TRACK_GRAY,
                                     INDICATOR_TRACK_GRAY, track_alpha));

    /* a hairline separates the track from the content */
    const int border = (int)std::max(Layout::VptScale(INDICATOR_BORDER_PT), 1u);
    canvas.DrawFilledRectangle(PixelRect{track.GetTopLeft(),
                                         PixelPoint{track.left + border,
                                                    track.bottom}},
                               Color(INDICATOR_TRACK_BORDER_GRAY,
                                     INDICATOR_TRACK_BORDER_GRAY,
                                     INDICATOR_TRACK_BORDER_GRAY,
                                     track_alpha));
  }

  /* the grabbed bar is darker, like the one macOS shows on hover */
  const uint8_t gray = wide ? 0x60 : 0x80;
  indicator_brush.Create(Color(gray, gray, gray, indicator_alpha));
#else
  /* this canvas cannot blend a translucent fill; a plain gray is
     legible on both light and dark backgrounds */
  if (!indicator_brush.IsDefined())
    indicator_brush.Create(IsDithered() ? COLOR_BLACK : COLOR_GRAY);
#endif

  canvas.SelectNullPen();
  canvas.Select(indicator_brush);

  const unsigned diameter = rc_thumb.GetWidth();
  canvas.DrawRoundRectangle(rc_thumb, PixelSize{diameter, diameter});
}

void
ScrollBar::PaintBar(Canvas &canvas, ButtonState up_state,
                    ButtonState down_state) noexcept
{
  // draw rectangle around entire scrollbar area
  canvas.SelectBlackPen();
  canvas.SelectHollowBrush();
  canvas.DrawRectangle(rc);

  // draw the up/down arrow buttons
  const int arrow_padding = std::max(GetWidth() / 4, 4);

  PixelRect up_arrow_rect = rc;
  ++up_arrow_rect.left;
  up_arrow_rect.bottom = up_arrow_rect.top + GetWidth();

  PixelRect down_arrow_rect = rc;
  ++down_arrow_rect.left;
  down_arrow_rect.top = down_arrow_rect.bottom - GetWidth();

  canvas.DrawExactLine(up_arrow_rect.GetBottomLeft(),
                       up_arrow_rect.GetBottomRight());
  canvas.DrawExactLine({down_arrow_rect.left, down_arrow_rect.top - 1},
                       {down_arrow_rect.right, down_arrow_rect.top - 1});

  button_renderer.DrawButton(canvas, up_arrow_rect, up_state);
  button_renderer.DrawButton(canvas, down_arrow_rect, down_state);

  const ButtonLook &look = button_renderer.GetLook();
  canvas.SelectNullPen();

  const auto select_foreground = [&canvas, &look](ButtonState state) {
    switch (state) {
    case ButtonState::DISABLED:
      canvas.Select(look.disabled.brush);
      break;
    case ButtonState::FOCUSED:
    case ButtonState::PRESSED:
      /* match button rendering: focused and pressed share the same palette */
      canvas.Select(look.focused.foreground_brush);
      break;
    case ButtonState::SELECTED:
      canvas.Select(look.selected.foreground_brush);
      break;
    case ButtonState::ENABLED:
      canvas.Select(look.standard.foreground_brush);
      break;
    default:
      gcc_unreachable();
    }
  };

  const BulkPixelPoint up_arrow[3] = {
    { (up_arrow_rect.left + rc.right) / 2,
      up_arrow_rect.top + arrow_padding },
    { up_arrow_rect.left + arrow_padding,
      up_arrow_rect.bottom - arrow_padding },
    { rc.right - arrow_padding,
      up_arrow_rect.bottom - arrow_padding },
  };
  select_foreground(up_state);
  canvas.DrawTriangleFan(up_arrow, ARRAY_SIZE(up_arrow));

  const BulkPixelPoint down_arrow[3] = {
    { (down_arrow_rect.left + rc.right) / 2,
      down_arrow_rect.bottom - arrow_padding },
    { down_arrow_rect.left + arrow_padding,
      down_arrow_rect.top + arrow_padding },
    { rc.right - arrow_padding,
      down_arrow_rect.top + arrow_padding },
  };
  select_foreground(down_state);
  canvas.DrawTriangleFan(down_arrow, ARRAY_SIZE(down_arrow));

  // ###################
  // ####  Slider   ####
  // ###################

  if (rc_slider.top + 4 < rc_slider.bottom) {
    canvas.SelectBlackPen();
    canvas.DrawExactLine(rc_slider.GetTopLeft(), rc_slider.GetTopRight());
    canvas.DrawExactLine(rc_slider.GetBottomLeft(),
                         rc_slider.GetBottomRight());

    PixelRect rc_slider2 = rc_slider;
    ++rc_slider2.left;
    ++rc_slider2.top;
    button_renderer.DrawButton(canvas, rc_slider2,
                               dragging ? ButtonState::PRESSED : ButtonState::ENABLED);
  }

  // fill the rest with darker gray
  const Color background_color = IsDithered() ? COLOR_BLACK : COLOR_GRAY;

  if (up_arrow_rect.bottom + 1 < rc_slider.top)
    canvas.DrawFilledRectangle({rc.left + 1, up_arrow_rect.bottom + 1, rc.right, rc_slider.top},
                               background_color);

  if (rc_slider.bottom + 1 < down_arrow_rect.top - 1)
    canvas.DrawFilledRectangle({rc.left + 1, rc_slider.bottom + 1, rc.right, down_arrow_rect.top - 1},
                               background_color);
}

void
ScrollBar::DragBegin(PaintWindow *w, unsigned y) noexcept
{
  // Make sure that we are not dragging already
  assert(!dragging);

  // Save the offset of the drag
  drag_offset = y - rc_slider.top;
  // ... and remember that we are dragging now
  dragging = true;

  if (style == Style::WHEN_SCROLLING) {
    /* hold the indicator while it is being dragged; it grows, so that
       the finger does not cover it completely */
    indicator_alpha = INDICATOR_ALPHA;
    indicator_wide = true;
    indicator_timer.Cancel();
  }

  w->SetCapture();
  w->Invalidate(IsReservingSpace() ? rc_slider : GetIndicatorTrack());
}

void
ScrollBar::DragEnd(PaintWindow *w) noexcept
{
  // If we are not dragging right now -> nothing to end
  if (!dragging)
    return;

  // Realize that we are not dragging anymore
  dragging = false;

  if (style == Style::WHEN_SCROLLING) {
    /* let it fade out again, keeping its wide shape; the next mouse
       move restores the hover state if the pointer is still on it */
    indicator_hover = false;
    indicator_timer.Schedule(INDICATOR_HOLD);
  }

  w->ReleaseCapture();
  w->Invalidate(IsReservingSpace() ? rc_slider : GetIndicatorTrack());
}

unsigned
ScrollBar::DragMove(unsigned size, unsigned view_size, int y) const noexcept
{
  assert(dragging);

  return ToOrigin(size, view_size, y - drag_offset);
}
