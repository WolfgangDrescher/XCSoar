// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Restart XCSoar without leaving the process: the main loop shuts the
 * program down and boots it up again.  Settings which are evaluated
 * only during startup can thus be applied without terminating the
 * application, which is cumbersome (Android) or impossible (iOS) for
 * the user.
 */

#ifndef USE_WAYLAND
/**
 * Can XCSoar restart itself inside this process?  This requires a
 * windowing backend whose native window survives (or can be created
 * again by) the new session.
 */
#define HAVE_IN_PROCESS_RESTART
#endif

/**
 * Ask the main loop to restart XCSoar.  This leaves the event loop
 * (including the nested loops of all dialogs which are currently
 * open) without asking the "Quit program?" question; the restart
 * itself happens after Shutdown() has finished.
 *
 * Does nothing if #HAVE_IN_PROCESS_RESTART is not defined.
 */
void
RequestRestart() noexcept;

/**
 * Tell the user that settings which were just saved take effect on
 * the next start only, and offer to restart XCSoar right away.
 */
void
ShowRestartRequiredDialog() noexcept;

/**
 * Check and clear the flag set by RequestRestart(); called by the
 * main loop after Shutdown().
 *
 * @return true if XCSoar shall boot up again
 */
bool
ConsumeRestartRequest() noexcept;

/**
 * Was the current session started by RequestRestart()?  Startup() uses
 * this to skip prompts which the user has answered already.
 */
[[gnu::pure]]
bool
IsRestart() noexcept;
