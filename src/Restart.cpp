// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Restart.hpp"
#include "Interface.hpp"
#include "MainWindow.hpp"
#include "Dialogs/Message.hpp"
#include "Language/Language.hpp"
#include "LogFile.hpp"

static bool restart_requested = false;
static bool is_restart = false;

void
RequestRestart() noexcept
{
#ifdef HAVE_IN_PROCESS_RESTART
  if (CommonInterface::main_window == nullptr)
    return;

  restart_requested = true;

  /* PostQuit() (instead of MainWindow::Close()) leaves the event loop
     unconditionally: it asks no "Quit program?" question, and it also
     terminates the nested event loops of all dialogs which are still
     open */
  CommonInterface::main_window->PostQuit();
#endif
}

void
ShowRestartRequiredDialog() noexcept
{
#ifdef HAVE_IN_PROCESS_RESTART
  if (ShowMessageBox(_("Changes to configuration saved. Restart XCSoar now to apply changes?"),
                     "XCSoar", MB_YESNO | MB_ICONQUESTION) == IDYES)
    RequestRestart();
#else
  ShowMessageBox(_("Changes to configuration saved. Restart XCSoar to apply changes."),
                 "", MB_OK);
#endif
}

bool
ConsumeRestartRequest() noexcept
{
  if (!restart_requested)
    return false;

  restart_requested = false;
  is_restart = true;

  LogString("Restarting XCSoar");
  return true;
}

bool
IsRestart() noexcept
{
  return is_restart;
}
