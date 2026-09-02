// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/dim/Rect.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "Renderer/ButtonRenderer.hpp"

#include <algorithm>
#include <cstdint>

class PaintWindow;
class Canvas;

class ScrollBar {
public:
  /**
   * How a scroll bar presents itself.  This is a global user setting
   * (see #UISettings::scroll_bars), not a per-widget decision.
   */
  enum class Style : uint_least8_t {
    /**
     * A permanently visible scroll bar with arrow buttons.  It
     * reserves a column of the client area and can be operated with
     * the mouse.
     */
    ALWAYS,

    /**
     * A thin translucent indicator drawn on top of the content while
     * it is being scrolled, fading out afterwards.  It reserves no
     * space and cannot be dragged; the content itself is dragged
     * instead.
     */
    WHEN_SCROLLING,
  };

  /**
   * Sets the style for all scroll bars.  It takes effect the next
   * time a scroll bar is laid out (see #SetSize).
   */
  static void SetGlobalStyle(Style style) noexcept;

  [[gnu::pure]]
  static Style GetGlobalStyle() noexcept;

  /**
   * Returns the height of one scroll step ("one line") in pixels.  It
   * does not depend on the current style, so that scrolling by key or
   * mouse wheel is not affected by the appearance of the scroll bar.
   */
  [[gnu::pure]]
  static unsigned GetScrollStep() noexcept;

private:
  /** Returns the width of a #Style::ALWAYS scroll bar. */
  [[gnu::pure]]
  static unsigned GetBarWidth() noexcept;

  /** The window owning this scroll bar; repainted while fading out. */
  PaintWindow &window;

  ButtonFrameRenderer button_renderer;

  /** The style this scroll bar was laid out with (see #SetSize) */
  Style style = Style::ALWAYS;

  /**
   * #Style::WHEN_SCROLLING: the opacity of the indicator; 0 means it
   * is currently invisible.
   */
  uint8_t indicator_alpha = 0;

  /**
   * #Style::WHEN_SCROLLING: the brush for the indicator.  Where the
   * canvas can blend, it is recreated whenever #indicator_alpha
   * changes.
   */
  Brush indicator_brush;

  /**
   * #Style::WHEN_SCROLLING: holds the indicator visible after the
   * last scroll movement, and then fades it out.
   */
  UI::PeriodicTimer indicator_timer{[this]{ OnIndicatorTimer(); }};

  /**
   * #Style::WHEN_SCROLLING: the gap between the indicator and the
   * right edge of the window, and the one above and below it, so
   * that it does not touch the edges (see #SetSize).
   */
  unsigned indicator_margin = 0, indicator_padding = 0;

  /**
   * #Style::WHEN_SCROLLING: is the pointer resting on the indicator?
   * It then grows to the width of a real scroll bar, the way the
   * overlay scroll bars on macOS do, so it can be grabbed.
   */
  bool indicator_hover = false;

  /**
   * #Style::WHEN_SCROLLING: paint the indicator in its wide shape?
   * A hover or a drag sets this, and only the fade-out clears it: the
   * indicator keeps the shape it has until it is gone, instead of
   * snapping back to the thin bar while the user is still looking.
   */
  bool indicator_wide = false;

protected:
  /** Whether the slider is currently being dragged */
  bool dragging;
  int drag_offset;
  /** Coordinates of the ScrollBar */
  PixelRect rc;
  /** Coordinates of the Slider */
  PixelRect rc_slider;

public:
  /**
   * Constructor of the ScrollBar class
   *
   * @param window the window this scroll bar is painted on; it is
   * invalidated while the #Style::WHEN_SCROLLING indicator fades out
   */
  ScrollBar(PaintWindow &window, const ButtonLook &button_look) noexcept;

  ~ScrollBar() noexcept;

  /** Returns the width of the ScrollBar */
  int GetWidth() const noexcept {
    return rc.GetWidth();
  }

  /** Returns the height of the ScrollBar */
  int GetHeight() const noexcept {
    return rc.GetHeight();
  }

  /** Returns the height of the slider */
  int GetSliderHeight() const noexcept {
    return rc_slider.GetHeight();
  }

  /**
   * Returns the height of one arrow button.  The #Style::WHEN_SCROLLING
   * indicator has no arrows, and scrolls over the full height.
   */
  int GetArrowHeight() const noexcept {
    return IsReservingSpace() ? GetWidth() : 0;
  }

  /** Returns the height of the scrollable area of the ScrollBar */
  int GetNettoHeight() const noexcept {
    return std::max(GetHeight() - 2 * GetArrowHeight() - 1, 0);
  }

  /**
   * Returns the height of the visible scroll area of the ScrollBar
   * (the area thats not covered with the slider)
   */
  int GetScrollHeight() const noexcept {
    return std::max(GetNettoHeight() - GetSliderHeight(), 1);
  }

  /**
   * Returns whether the ScrollBar is defined or has to be set up first
   * @return True if the ScrollBar is defined,
   * False if it has to be set up first
   */
  bool IsDefined() const noexcept {
    return GetWidth() > 0;
  }

  /**
   * Returns whether this scroll bar occupies a column of the client
   * area.  A #Style::WHEN_SCROLLING indicator floats above the
   * content and does not.
   */
  constexpr bool IsReservingSpace() const noexcept {
    return style == Style::ALWAYS;
  }

  /**
   * Returns the x-Coordinate of the ScrollBar
   * (remaining client area aside the ScrollBar)
   * @param size Size of the client area including the ScrollBar
   * @return The x-Coordinate of the ScrollBar
   */
  unsigned GetLeft(const PixelSize size) const noexcept {
    return IsDefined() && IsReservingSpace() ? rc.left : size.width;
  }

  /**
   * Returns whether the given PixelPoint is in the ScrollBar area
   * @param pt PixelPoint to check
   * @return True if the given PixelPoint is in the ScrollBar area,
   * False otherwise
   */
  bool IsInside(const PixelPoint &pt) const noexcept {
    return IsReservingSpace() && rc.Contains(pt);
  }

  /**
   * Returns whether the given PixelPoint grabs the slider.  The
   * #Style::WHEN_SCROLLING indicator is only a few pixels wide, but
   * it can be grabbed from anywhere in its column, so that a finger
   * can hit it too; it must be visible at the time.
   *
   * @param pt PixelPoint to check
   */
  [[gnu::pure]]
  bool IsInsideSlider(PixelPoint pt) const noexcept;

  /**
   * Returns whether the given y-Coordinate is on the up arrow
   * @param y y-Coordinate to check
   * @return True if the given y-Coordinate is on the up arrow,
   * False otherwise
   */
  bool IsInsideUpArrow(int y) const noexcept {
    return y < rc.top + GetWidth();
  }

  /**
   * Returns whether the given y-Coordinate is on the down arrow
   * @param y y-Coordinate to check
   * @return True if the given y-Coordinate is on the down arrow,
   * False otherwise
   */
  bool IsInsideDownArrow(int y) const noexcept {
    return y >= rc.bottom - GetWidth();
  }

  /**
   * Returns whether the given y-Coordinate is above the slider area
   * @param y y-Coordinate to check
   * @return True if the given y-Coordinate is above the slider area,
   * False otherwise
   */
  bool IsAboveSlider(int y) const noexcept {
    return y < rc_slider.top;
  }

  /**
   * Returns whether the given y-Coordinate is below the slider area
   * @param y y-Coordinate to check
   * @return True if the given y-Coordinate is below the slider area,
   * False otherwise
   */
  bool IsBelowSlider(int y) const noexcept {
    return y >= rc_slider.bottom;
  }

  /**
   * Sets the size of the ScrollBar
   * (actually just the height, width is automatically set)
   * @param size Size of the Control the ScrollBar is used with
   */
  void SetSize(const PixelSize size) noexcept;

  /** Resets the ScrollBar (undefines it) */
  void Reset() noexcept;

  /**
   * Must be called whenever the content was scrolled.  In
   * #Style::WHEN_SCROLLING, this shows the indicator and restarts its
   * fade-out; in #Style::ALWAYS it does nothing.
   */
  void NotifyScroll() noexcept;

  /**
   * Reports the current mouse position, so the
   * #Style::WHEN_SCROLLING indicator can appear and grow while the
   * pointer rests on it (this is what macOS does).  Call this only
   * while the content is not being dragged, because a touch drag
   * passing the column is not a hover.
   */
  void NotifyMouseMove(PixelPoint pt) noexcept;

  /**
   * Hides the #Style::WHEN_SCROLLING indicator immediately and stops
   * the fade-out animation.  Call this from OnDestroy(), because the
   * animation repaints the window.
   */
  void HideIndicator() noexcept;

  /** Calculates the size and position of the slider */
  void SetSlider(unsigned size, unsigned view_size, unsigned origin) noexcept;

  /** Calculates the new origin out of the given y-Coordinate of the drag */
  unsigned ToOrigin(unsigned size, unsigned view_size, int y) const noexcept;

  /** Paints the ScollBar */
  void Paint(Canvas &canvas) noexcept;

  /**
   * Paints the ScrollBar with explicit button states.
   *
   * @param up_state State of the up arrow button.
   * @param down_state State of the down arrow button.
   */
  void Paint(Canvas &canvas, ButtonState up_state,
             ButtonState down_state) noexcept;

  /**
   * Returns whether the slider is currently being dragged
   * @return True if the slider is currently being dragged, False otherwise
   */
  bool IsDragging() const noexcept {
    return dragging;
  }

  /**
   * Should be called when beginning to drag
   * (Called by ListControl::OnMouseDown)
   * @param w The Window object the ScrollBar is belonging to
   * @param y y-Coordinate
   */
  void DragBegin(PaintWindow *w, unsigned y) noexcept;

  /**
   * Should be called when stopping to drag
   * (Called by ListControl::OnMouseUp)
   * @param w The Window object the ScrollBar is belonging to
   */
  void DragEnd(PaintWindow *w) noexcept;

  /**
   * Should be called while dragging
   * @param size Size of the Scrollbar (not pixelwise)
   * @param view_size Visible size of the Scrollbar (not pixelwise)
   * @param y y-Coordinate
   * @return "Value" of the ScrollBar
   */
  unsigned DragMove(unsigned  size, unsigned view_size, int y) const noexcept;

private:
  /**
   * Is the #Style::WHEN_SCROLLING indicator currently showing its
   * wide shape, i.e. is it hovered or dragged?
   */
  constexpr bool IsIndicatorWide() const noexcept {
    return indicator_wide;
  }

  /**
   * The track of the #Style::WHEN_SCROLLING indicator: the area it is
   * painted in, and the one the pointer hovers it in.
   */
  [[gnu::pure]]
  PixelRect GetIndicatorTrack() const noexcept;

  /**
   * The area the #Style::WHEN_SCROLLING indicator can be grabbed in.
   * It is wider than the bar and reaches past its ends, so that a
   * finger can hit it.
   */
  [[gnu::pure]]
  PixelRect GetIndicatorGrabRect() const noexcept;

  /** The shape the #Style::WHEN_SCROLLING indicator is painted in */
  [[gnu::pure]]
  PixelRect GetIndicatorRect() const noexcept;

  /** Repaints the area the indicator may occupy */
  void InvalidateIndicator() noexcept;

  /** Paints the #Style::ALWAYS scroll bar */
  void PaintBar(Canvas &canvas, ButtonState up_state,
                ButtonState down_state) noexcept;

  /** Paints the #Style::WHEN_SCROLLING indicator */
  void PaintIndicator(Canvas &canvas) noexcept;

  void OnIndicatorTimer() noexcept;
};
