// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Let the pilot switch the status bar on and off, choose its style
 * and its items, and on iOS, whether the system status bar is
 * visible.
 *
 * @return true if a setting has changed
 */
bool
ShowStatusBarDialog() noexcept;

/**
 * Describe the status bar settings in a word, e.g. "Off" or "White",
 * for the row which opens ShowStatusBarDialog().
 */
[[gnu::pure]]
const char *
GetStatusBarSummary() noexcept;
