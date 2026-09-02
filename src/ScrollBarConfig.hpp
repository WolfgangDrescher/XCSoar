// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct UISettings;

/**
 * Resolves UISettings::scroll_bars (which may be
 * UISettings::ScrollBars::AUTO) and applies the result to all scroll
 * bars.
 *
 * Call this after the display type is known (see #SetDisplayType),
 * because e-paper screens cannot show a fading indicator.
 */
void
ApplyScrollBars(const UISettings &settings) noexcept;
