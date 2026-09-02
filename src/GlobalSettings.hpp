// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

/**
 * This namespace provides access to configuration settings in the
 * operating system.  This allows XCSoar to inherit global settings.
 */
namespace GlobalSettings {

#ifdef ANDROID
inline bool dark_mode = false;
inline bool haptic_feedback = false;
#else
static constexpr bool dark_mode = false;
#endif

/**
 * The operating system's preference for showing scroll bars.
 */
enum class ScrollBars : uint_least8_t {
  /** the operating system has no such setting (or it is unreadable) */
  UNKNOWN,

  /** show a thin overlay indicator only while scrolling */
  WHEN_SCROLLING,

  /** always show a scroll bar */
  ALWAYS,
};

#ifdef __APPLE__

/**
 * Reads the operating system's scroll bar preference.
 *
 * On macOS, this is "Show scroll bars" in System Settings.  iOS has
 * no such setting; there, "Automatic" is answered the way macOS does
 * it, by looking for a mouse or trackpad.
 */
ScrollBars
GetScrollBars() noexcept;

#else

constexpr ScrollBars
GetScrollBars() noexcept
{
  return ScrollBars::UNKNOWN;
}

#endif

} // namespace GlobalSettings
