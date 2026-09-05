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

  const InfoBoxLook &look;
  const DialogLook &dialog_look;

  InfoBoxSettings::Panel *panel = nullptr;
  const InfoBoxLayout::Layout *layout = nullptr;

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

private:
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

  void DrawCard(Canvas &canvas, const PixelRect &rc, unsigned slot,
                unsigned number, bool active) noexcept;

  void PaintCards(Canvas &canvas) noexcept;
  void PaintButtons(Canvas &canvas) noexcept;
  void PaintPanelName(Canvas &canvas) noexcept;
  void PaintDescription(Canvas &canvas) noexcept;

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
  void OnCancelMode() noexcept override;
};
