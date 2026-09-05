// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxSettings.hpp"
#include "Renderer/TextButtonRenderer.hpp"
#include "Renderer/TextRenderer.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/PaintWindow.hpp"
#include "util/StaticArray.hxx"

#include <chrono>
#include <functional>
#include <optional>

struct DialogLook;
struct InfoBoxLook;

namespace InfoBoxLayout { struct Layout; }

/**
 * Shows the InfoBoxes of one panel as cards which the user can
 * exchange by dragging them, with the cursor keys or with a remote
 * stick.  Tapping a card describes its InfoBox, and a long press
 * chooses a different one.
 *
 * The window paints everything it shows, which is why the card which
 * follows the finger can never end up behind another window.
 */
class InfoBoxArrangeWindow : public PaintWindow {
public:
  using Callback = std::function<void()>;

  static constexpr unsigned MAX_BUTTONS = 2;

private:
  /** How is the InfoBox the user is working with drawn? */
  enum class CardState {
    /** just one of the InfoBoxes */
    NORMAL,

    /**
     * Selected with the cursor keys.  Like a focused button, the card
     * only gets a ring, because it is not taken yet.
     */
    FOCUSED,

    /** tapped, taken with Enter, or following the finger */
    ACTIVE,
  };

  /** How was the InfoBox which is being worked with selected? */
  enum class Selection {
    /** by tapping it, or by grabbing it with the finger */
    TOUCH,

    /** with the cursor keys */
    FOCUSED,

    /** taken with Enter, ready to be moved with the cursor keys */
    MOVING,
  };

  /** One of the buttons below the cards. */
  struct ButtonItem {
    const char *caption;
    Callback callback;
  };

  /** The InfoBox which is being dragged. */
  struct DragState {
    /** the slot the dragged InfoBox occupies right now */
    unsigned slot;

    /** the slot rectangle the drag started from */
    PixelRect start_rect;

    /** the press position in parent coordinates when the drag started */
    PixelPoint origin;

    /** the current pointer position in parent coordinates */
    PixelPoint pointer;

    /** does the InfoBox follow the finger already? */
    bool following;
  };

  /**
   * An InfoBox which was displaced by the dragged one slides into its
   * new slot instead of jumping there.
   */
  struct Shuffle {
    /** where the card starts, relative to its slot */
    PixelPoint offset{0, 0};

    std::chrono::steady_clock::time_point start{};
  };

  /**
   * Sort key for the Tab order: it follows the axis the InfoBoxes are
   * stacked on, so row by row when they stand in rows and column by
   * column when they stand in columns, and the buttons take their
   * place in that order by where they sit on the screen.
   */
  struct TabKey {
    /** the row or column on the screen */
    int group;

    /** the place inside that group */
    int position;

    constexpr bool operator<(const TabKey &other) const noexcept {
      return group != other.group
        ? group < other.group
        : position < other.position;
    }
  };

  const InfoBoxLook &look;
  const DialogLook &dialog_look;

  InfoBoxSettings::Panel *panel = nullptr;
  const InfoBoxLayout::Layout *layout = nullptr;

  /** are the InfoBoxes of #layout arranged in columns? */
  bool columns = false;

  /** the area between the cards, for the description and the buttons */
  PixelRect content;

  StaticArray<ButtonItem, MAX_BUTTONS> buttons;

  /** draws all buttons, one after the other */
  TextButtonRenderer button_renderer;

  std::optional<DragState> drag;

  /** the InfoBox configuration as it was when the drag started */
  InfoBoxSettings::Panel drag_snapshot;

  /** the button the press started on, or -1 */
  int held_button = -1;

  /** the button the cursor keys have selected, or -1 */
  int focused_button = -1;

  /**
   * The place across the axis the cursor came from when it entered
   * the button row, so that crossing the row does not drop it in the
   * middle of the layout.
   */
  int button_row_cross = 0;

  /** is the finger still on #held_button? */
  bool button_down = false;

  Shuffle shuffle[InfoBoxSettings::Panel::MAX_CONTENTS];

  /**
   * The number each slot shows.  While dragging, the numbers travel
   * with the InfoBoxes instead of staying on the slots, so that they
   * only change once the finger is lifted.
   */
  unsigned card_number[InfoBoxSettings::Panel::MAX_CONTENTS];

  /** the InfoBox whose description is shown, or -1 */
  int described_slot = -1;

  /** how #described_slot was selected */
  Selection selection = Selection::TOUCH;

  /** the slot #Selection::MOVING started from */
  unsigned grab_slot;

  TextRenderer name_renderer, description_renderer;

  /** opens the InfoBox picker when an InfoBox is held down */
  UI::Timer picker_timer{[this]{ OnPickerTimer(); }};

  UI::PeriodicTimer shuffle_timer{[this]{ OnShuffleTimer(); }};

public:
  InfoBoxArrangeWindow(const InfoBoxLook &_look,
                       const DialogLook &_dialog_look) noexcept;

  /**
   * Add a button below the cards, to the right of the previous one.
   * Only before Create().
   */
  void AddButton(const char *caption, Callback callback) noexcept;

  void Create(ContainerWindow &parent, const PixelRect &rc) noexcept;

  /** Which panel does the user arrange? */
  void SetPanel(InfoBoxSettings::Panel &_panel) noexcept;

  /**
   * @param _layout where the cards are
   * @param _content the area between them, for the description and the
   * buttons; usually InfoBoxLayout::Layout::remaining, less what the
   * caller puts there itself
   */
  void SetLayout(const InfoBoxLayout::Layout &_layout,
                 PixelRect _content) noexcept;

  /** Select an InfoBox with no drag, for the cursor keys. */
  void FocusSlot(unsigned slot) noexcept;

  /** Begin a drag which was started on another window. */
  void BeginDrag(unsigned slot, PixelPoint pointer, bool follow) noexcept;

  /** Abort the drag and undo the exchanges it has made. */
  void CancelDrag() noexcept;

  /** Finish a drag which is still in progress. */
  void Drop() noexcept;

  /** Explain how an InfoBox is moved and how it is replaced. */
  void ShowHelp() noexcept;

protected:
  /**
   * The user has done something; the map overlay restarts its timeout.
   */
  virtual void OnArrangeActivity() noexcept {}

  /**
   * A modal dialog is about to cover this window; the map overlay
   * stops its timeout until the next OnArrangeActivity().
   */
  virtual void OnArrangeSuspend() noexcept {}

  /**
   * The Escape key was pressed.
   *
   * @return true if this window has handled it
   */
  virtual bool OnArrangeCancel() noexcept {
    return false;
  }

private:
  [[gnu::pure]]
  PixelPoint SlotCenter(unsigned slot) const noexcept;

  /** The coordinate along the axis the InfoBoxes are stacked on. */
  [[gnu::pure]]
  int Along(PixelPoint p) const noexcept;

  /** The coordinate across that axis. */
  [[gnu::pure]]
  int Across(PixelPoint p) const noexcept;

  /** Which slot covers the given position in parent coordinates? */
  [[gnu::pure]]
  int FindSlot(PixelPoint p) const noexcept;

  /** Where does the card which follows the finger sit? */
  [[gnu::pure]]
  PixelRect GetFloatingRect() const noexcept;

  /** The gap between the buttons and around the button row. */
  [[gnu::pure]]
  static int GetButtonGap() noexcept;

  /** How wide is a button?  They all have the same width. */
  [[gnu::pure]]
  int GetButtonWidth() const noexcept;

  /** The row which holds the buttons. */
  [[gnu::pure]]
  PixelRect GetButtonRowRect() const noexcept;

  /** One of the buttons in #GetButtonRowRect(). */
  [[gnu::pure]]
  PixelRect GetButtonRect(int i) const noexcept;

  /** Which button covers the given position in parent coordinates? */
  [[gnu::pure]]
  int FindButton(PixelPoint p) const noexcept;

  /** Where the name of the panel is drawn, above the buttons. */
  [[gnu::pure]]
  PixelRect GetPanelNameRect() const noexcept;

  [[gnu::pure]]
  PixelRect ToLocal(PixelRect rc) const noexcept;

  [[gnu::pure]]
  ButtonState GetButtonState(int i) const noexcept;

  [[gnu::pure]]
  CardState GetCardState(unsigned slot) const noexcept;

  void DrawCard(Canvas &canvas, const PixelRect &rc, unsigned slot,
                unsigned number, CardState state) noexcept;

  void PaintCards(Canvas &canvas) noexcept;
  void PaintButtons(Canvas &canvas) noexcept;
  void PaintPanelName(Canvas &canvas) noexcept;
  void PaintDescription(Canvas &canvas) noexcept;

  /** Exchange the InfoBoxes in two slots. */
  void Exchange(unsigned a, unsigned b) noexcept;

  void ResetCardNumbers() noexcept;

  /** Let the InfoBox in @p slot slide in from @p from. */
  void StartShuffle(unsigned slot, const PixelRect &from) noexcept;

  /** How far is the InfoBox in @p slot still away from its slot? */
  [[gnu::pure]]
  PixelPoint GetShuffleOffset(unsigned slot) const noexcept;

  void OnShuffleTimer() noexcept;

  /** Open the picker for the InfoBox which is being held down. */
  void OnPickerTimer() noexcept;

  /** Let the user choose a different InfoBox for @p slot. */
  void ShowPicker(unsigned slot) noexcept;

  /**
   * Where the cursor leaves the button row: on the row itself, but at
   * the place across the axis it came from.
   */
  [[gnu::pure]]
  PixelPoint GetButtonRowOrigin() const noexcept;

  /**
   * Which slot lies next to @p origin in the direction (@p dx, @p dy)?
   *
   * @return the slot, or -1 if there is none in that direction
   */
  [[gnu::pure]]
  int FindNeighbour(PixelPoint origin, int dx, int dy) const noexcept;

  /**
   * Does the button row come before @p slot when moving away from
   * @p origin?
   */
  [[gnu::pure]]
  bool IsButtonRowCloser(PixelPoint origin, int slot,
                         bool forward) const noexcept;

  /** Focus the button which is closest to @p origin. */
  void FocusButtonNear(PixelPoint origin) noexcept;

  /** Move the cursor away from the button row. */
  bool MoveFromButton(int dx, int dy, bool along) noexcept;

  [[gnu::pure]]
  TabKey GetTabKey(unsigned i) const noexcept;

  void FocusItem(unsigned i) noexcept;

  /**
   * Move the focus one step along the Tab order, wrapping around at
   * the ends.
   */
  bool MoveTab(bool forward) noexcept;

  /** Move the selection with the cursor keys or a remote stick. */
  bool MoveSelection(int dx, int dy) noexcept;

  /**
   * Enter takes the selected InfoBox, puts it down again, or opens
   * the picker if it was put down where it was taken.
   */
  bool Activate() noexcept;

  /** Run what the button in @p i does. */
  void OnButton(int i) noexcept;

  void HoldButton(int i, bool down) noexcept;
  void ReleaseButton() noexcept;

  /** Move the dragged card; @p p is in parent coordinates. */
  void Drag(PixelPoint p) noexcept;

protected:
  /* virtual methods from class PaintWindow */
  void OnPaint(Canvas &canvas) noexcept override;

  /* virtual methods from class Window */
  bool OnMouseDown(PixelPoint p) noexcept override;
  bool OnMouseMove(PixelPoint p, unsigned keys) noexcept override;
  bool OnMouseUp(PixelPoint p) noexcept override;
  bool OnMouseDouble(PixelPoint p) noexcept override;
#ifdef HAVE_MULTI_TOUCH
  bool OnMultiTouchDown() noexcept override;
#endif
  bool OnKeyCheck(unsigned key_code) const noexcept override;
  bool OnKeyDown(unsigned key_code) noexcept override;
  void OnCancelMode() noexcept override;
};
