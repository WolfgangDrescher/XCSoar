// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/window/PaintWindow.hpp"
#include "ui/canvas/Color.hpp"
#include "ui/canvas/Font.hpp"
#include "UISettings.hpp"
#include "Screen/Layout.hpp"

#include <array>
#include <memory>

class Canvas;
class InfoBoxContent;
struct StatusBarItem;

/**
 * A narrow bar at the top of the screen: the position and the short
 * title of the page on the left, the state of the data source and the
 * devices on the right.  A tap opens the status dialog.
 */
class StatusBarWindow final : public PaintWindow {
  const Font &font;

  /**
   * A smaller font for the units of the InfoBox values.
   */
  Font unit_font;

  /**
   * The font of the charge inside the battery.
   */
  Font battery_font;

  const UISettings::StatusBarStyle style;

  /**
   * How far the items stay away from the left and the right edge,
   * see SetItemInsets().
   */
  int inset_left = 0, inset_right = 0;

  bool pressed = false;

  /**
   * The item which was pressed, or -1 for the bar next to the items.
   */
  int pressed_item = -1;

  /**
   * Is DrawItem() only measuring the items, without drawing them?
   */
  bool measuring = false;

  /**
   * Shall the data source be abbreviated ("SIM"), to save room?
   */
  bool source_abbreviated = false;

  /**
   * The InfoBox contents which compute the values of the items (see
   * UISettings::status_bar_items), by the index of the item, and
   * their types.
   */
  std::array<unsigned, StatusBarItems::MAX_ITEMS> value_types;
  std::array<std::unique_ptr<InfoBoxContent>,
             StatusBarItems::MAX_ITEMS> value_contents;

  /**
   * The items drawn last, and the widths of their texts, which grow
   * but do not shrink, so that the bar does not wobble.
   */
  StatusBarItems item_list;

  /**
   * Where the items were drawn last, for the taps; empty for those
   * which were left out.
   */
  std::array<PixelRect, StatusBarItems::MAX_ITEMS> item_rects;
  std::array<int, StatusBarItems::MAX_ITEMS> item_widths;

public:
  StatusBarWindow(const Font &_font,
                  UISettings::StatusBarStyle _style) noexcept;
  ~StatusBarWindow() noexcept;

  UISettings::StatusBarStyle GetStyle() const noexcept {
    return style;
  }

  void Create(ContainerWindow &parent, PixelRect rc) noexcept;

  /**
   * Keep the items this far away from the left and the right edge,
   * e.g. clear of the display cutout and the rounded corners, while
   * the bar itself reaches the screen border.
   */
  void SetItemInsets(int left, int right) noexcept;

  /**
   * The height of the bar in the given font.
   */
  [[gnu::pure]]
  static unsigned GetHeight(const Font &font) noexcept;

private:
  /**
   * Draw an item at the left or right edge of the given zone.
   *
   * @return the width of the item, 0 if it has nothing to show, or
   * -1 if it does not fit into the zone
   */
  int DrawItem(Canvas &canvas, unsigned i, StatusBarItem item,
               const PixelRect &zone, bool from_left,
               int text_top, Color text_color,
               Color background_color) noexcept;

  int DrawPageNumber(Canvas &canvas, unsigned i, const PixelRect &zone,
                     bool from_left, Color text_color,
                     Color background_color) noexcept;

  void SetItemRect(unsigned i, PixelRect rc) noexcept {
    /* a symbol is small for a finger: accept a tap next to it, too */
    rc.left -= Layout::Scale(3);
    rc.right += Layout::Scale(3);
    item_rects[i] = rc;
  }

  /**
   * The index of the item at the given position, or -1.
   */
  [[gnu::pure]]
  int GetItemAt(PixelPoint p) const noexcept;

  /**
   * Open what belongs to the given item, e.g. the replay dialog for
   * the data source during a replay, or the status dialog.
   */
  void OnItemClicked(int i) noexcept;

protected:
  /* virtual methods from class Window */
  bool OnMouseDown(PixelPoint p) noexcept override;
  bool OnMouseUp(PixelPoint p) noexcept override;

  /* virtual methods from class PaintWindow */
  void OnPaint(Canvas &canvas) noexcept override;
};
