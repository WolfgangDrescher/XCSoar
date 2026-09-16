// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxArrange.hpp"
#include "InfoBoxArrangeWindow.hpp"
#include "InfoBoxLayout.hpp"
#include "InfoBoxManager.hpp"
#include "InfoBoxWindow.hpp"
#include "Form/ButtonPanel.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Look/DialogLook.hpp"
#include "Look/Look.hpp"
#include "UIGlobals.hpp"
#include "UIState.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/ContainerWindow.hpp"
#include "ui/window/SingleWindow.hpp"

#include <memory>

using namespace UI;

namespace {

/** the InfoBox configuration as it was when the mode was entered */
InfoBoxSettings::Panel saved_panel;
unsigned saved_panel_index;

/**
 * Full-screen overlay: cards plus a real Help/Close #ButtonPanel.
 * Hidden (not destroyed) when the mode ends, because that happens
 * from its own event handler.  The inactivity timeout is the same as
 * the main menu.
 */
class OverlayWindow final : public ContainerWindow {
  class ArrangeWindow final : public InfoBoxArrangeWindow {
    OverlayWindow &overlay;

  public:
    explicit ArrangeWindow(OverlayWindow &_overlay) noexcept
      :InfoBoxArrangeWindow(UIGlobals::GetLook().info_box,
                            UIGlobals::GetDialogLook(),
                            Style::MAP),
       overlay(_overlay) {}

  protected:
    void OnArrangeActivity() noexcept override {
      overlay.RestartTimeout();
    }

    void OnArrangeSuspend() noexcept override {
      overlay.timeout_timer.Cancel();
    }

    bool OnArrangeCancel() noexcept override {
      InfoBoxArrange::Cancel();
      return true;
    }
  };

  ArrangeWindow arrange;
  ButtonPanel buttons;
  UI::Timer timeout_timer{[this]{ InfoBoxArrange::Save(); }};

  void RestartTimeout() noexcept {
    timeout_timer.Schedule(CommonInterface::GetUISettings().menu_timeout);
  }

  static void HideInfoBoxes() noexcept {
    for (unsigned i = 0; i < InfoBoxManager::layout.count; ++i)
      if (auto *window = InfoBoxManager::GetWindow(i))
        window->FastHide();
  }

  static void ShowInfoBoxes() noexcept {
    for (unsigned i = 0; i < InfoBoxManager::layout.count; ++i)
      if (auto *window = InfoBoxManager::GetWindow(i))
        window->Show();
  }

public:
  OverlayWindow() noexcept
    :arrange(*this),
     buttons(*this, UIGlobals::GetDialogLook().button) {}

  InfoBoxArrangeWindow &GetArrange() noexcept {
    return arrange;
  }

  void Create(SingleWindow &parent) noexcept {
    WindowStyle style;
    style.Hide();
    style.ControlParent();
    ContainerWindow::Create(parent, parent.GetClientRect(), style);

#ifndef USE_WINUSER
    /* the map below must still be painted */
    SetTransparent();
#endif

    arrange.Create(*this, GetClientRect());
    buttons.Add(_("Help"), [this]{ arrange.ShowHelp(); });
    buttons.Add(_("Close"), []{ InfoBoxArrange::Save(); });
  }

  void UpdateLayout() noexcept {
    arrange.Move(GetClientRect());
    arrange.SetLayout(InfoBoxManager::layout,
                      buttons.BottomLayout(InfoBoxManager::layout.remaining));
  }

  void StartTimeout() noexcept {
    RestartTimeout();
  }

  /** Show the overlay and hide the InfoBox windows behind it. */
  void Enter() noexcept {
    auto &parent = UIGlobals::GetMainWindow();
    if (IsDefined())
      Move(parent.GetClientRect());
    else
      Create(parent);

    arrange.SetPanel(InfoBoxManager::GetPanel(saved_panel_index));
    UpdateLayout();
    /* Create() hides the cards, as the settings dialog does; that
       dialog shows them from Widget::Show(), which this overlay
       never has */
    arrange.Show();
    Show();
    BringToTop();

    HideInfoBoxes();
    /* after hiding the InfoBoxes, so that a hidden one cannot keep
       the keyboard focus */
    arrange.SetFocus();
  }

  void Leave() noexcept {
    arrange.Drop();
    FocusParent();
    Hide();
    timeout_timer.Cancel();

    ShowInfoBoxes();
    InfoBoxManager::Refresh();
    InfoBoxManager::ScheduleRedraw();
  }

protected:
  void OnResize(PixelSize new_size) noexcept override {
    ContainerWindow::OnResize(new_size);

    if (arrange.IsDefined())
      UpdateLayout();
  }
};

std::unique_ptr<OverlayWindow> overlay;

[[gnu::pure]]
bool
IsShown() noexcept
{
  return overlay != nullptr && overlay->IsVisible();
}

void
Enter() noexcept
{
  /* display mode can switch the current panel while the overlay is
     up; save and cancel must still talk to the panel we opened */
  saved_panel_index = CommonInterface::GetUIState().panel_index;
  saved_panel = InfoBoxManager::GetPanel(saved_panel_index);
  if (overlay == nullptr)
    overlay = std::make_unique<OverlayWindow>();
  overlay->Enter();
}

} // namespace

bool
InfoBoxArrange::IsActive() noexcept
{
  return IsShown();
}

void
InfoBoxArrange::Begin(unsigned id, PixelPoint pointer) noexcept
{
  if (InfoBoxManager::GetWindow(id) == nullptr)
    return;

  if (!IsShown())
    Enter();

  /* the long press has already picked the InfoBox up, so it follows
     the finger right away */
  overlay->GetArrange().BeginDrag(id, pointer, true);
}

void
InfoBoxArrange::Begin() noexcept
{
  if (IsShown() || InfoBoxManager::GetWindow(0) == nullptr)
    return;

  Enter();
  overlay->GetArrange().FocusSlot(0);
  overlay->StartTimeout();
}

bool
InfoBoxArrange::SetFocus() noexcept
{
  if (!IsShown())
    return false;

  overlay->GetArrange().SetFocus();
  return true;
}

void
InfoBoxArrange::Save() noexcept
{
  if (!IsShown())
    return;

  overlay->Leave();
  InfoBoxManager::SavePanel(saved_panel_index);
}

void
InfoBoxArrange::Cancel() noexcept
{
  if (!IsShown())
    return;

  InfoBoxManager::GetPanel(saved_panel_index) = saved_panel;
  overlay->Leave();
}

void
InfoBoxArrange::Reset() noexcept
{
  overlay.reset();
}
