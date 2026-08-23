// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Message.hpp"
#include "PopupMessage.hpp"
#include "MainWindow.hpp"
#include "Interface.hpp"
#include "Apple/LocalNotifications.hpp"

#ifdef HAVE_LOCAL_NOTIFICATIONS
#include "StatusMessage.hpp"
#endif

void
Message::AddMessage(const char *text, const char *data) noexcept
{
  if (CommonInterface::main_window->popup != nullptr)
    CommonInterface::main_window->popup->AddMessage(text, data);

#ifdef HAVE_LOCAL_NOTIFICATIONS
  /* a message that is configured to be invisible shall not show up on
     the lock screen either */
  if (FindStatusMessage(text).visible)
    LocalNotifications::Post(text, data);
#endif
}
