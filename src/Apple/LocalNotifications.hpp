// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
/**
 * The operating system can show XCSoar messages on the lock screen
 * while XCSoar is not in the foreground.
 */
#define HAVE_LOCAL_NOTIFICATIONS
#endif
#endif

#ifdef HAVE_LOCAL_NOTIFICATIONS

namespace LocalNotifications {

/**
 * Set up the connection to the operating system's notification centre.
 * If @p enabled, the user is asked for permission to post
 * notifications; the operating system shows that dialog only once per
 * installation and remembers the answer.
 *
 * Must be called from the main thread.
 */
void
Initialise(bool enabled) noexcept;

/**
 * Undo Initialise().  Must be called from the main thread.
 */
void
Deinitialise() noexcept;

/**
 * Enable or disable notifications at runtime.  Must be called from the
 * main thread, because enabling may ask the user for permission.
 */
void
SetEnabled(bool enabled) noexcept;

/**
 * Post a message to the operating system's notification centre, unless
 * XCSoar is in the foreground; while the user is looking at the map,
 * the popup message is all that is needed.
 *
 * May be called from any thread.
 *
 * @param title the message text
 * @param body additional detail; may be nullptr
 */
void
Post(const char *title, const char *body) noexcept;

} // namespace LocalNotifications

#endif
