// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxSettings.hpp"
#include "util/StaticArray.hxx"

#include <optional>

namespace InfoBoxLayout {

/**
 * A pixel-precise InfoBox layout definition, loaded from a JSON file
 * (used by InfoBoxSettings::Geometry::CUSTOM).
 *
 * Each coordinate comes in one of two flavours:
 *
 *  - a plain number is a pixel value in the reference screen space
 *    (#screen_width / #screen_height); when the actual screen size
 *    differs from the reference, it is scaled proportionally
 *
 *  - a percentage (a JSON string such as "12.5%") is relative to the
 *    actual screen size and therefore resolution-independent
 */
struct CustomGeometry {
  struct Dimension {
    double value;

    /** true: #value is a percentage of the actual screen dimension;
        false: #value is pixels in reference screen space */
    bool percent;
  };

  struct Rect {
    Dimension x, y, width, height;
  };

  struct Box : Rect {
    /** bitmask of BORDERTOP/BORDERRIGHT/BORDERBOTTOM/BORDERLEFT */
    unsigned border;

    /**
     * Pinned content for this box (an InfoBoxFactory::Type value, as
     * also stored in the InfoBoxPanel<n>Box<i> profile keys), or
     * InfoBoxFactory::NUM_TYPES when the box shows the active InfoBox
     * set's content.  A pinned box ignores the set and cannot be
     * changed with the InfoBox picker.
     */
    InfoBoxFactory::Type content;

    constexpr bool HasContent() const noexcept {
      return content < InfoBoxFactory::NUM_TYPES;
    }
  };

  /** the screen size this layout was designed for */
  unsigned screen_width = 0, screen_height = 0;

  /** the (custom) DPI setting this layout was designed for
      (0 = unspecified) */
  unsigned screen_dpi = 0;

  /**
   * Apply this layout only when the actual screen size matches the
   * reference exactly, instead of scaling absolute coordinates.
   */
  bool strict = false;

  /** optional vario gauge area */
  std::optional<Rect> vario;

  /** optional explicit map area; when omitted, the largest screen
      rectangle not covered by any InfoBox is used */
  std::optional<Rect> map;

  StaticArray<Box, InfoBoxSettings::Panel::MAX_CONTENTS> boxes;
};

/**
 * Install (or clear) the default custom geometry which Calculate()
 * applies when InfoBoxSettings::Geometry::CUSTOM is selected and the
 * current InfoBox panel has no panel-specific custom geometry.
 */
void
SetCustomGeometry(std::optional<CustomGeometry> geometry) noexcept;

/**
 * Install (or clear) a custom geometry for one specific InfoBox
 * panel (set), overriding the default custom geometry for that
 * panel.
 */
void
SetPanelCustomGeometry(unsigned panel,
                       std::optional<CustomGeometry> geometry) noexcept;

/** the default custom geometry (or nullopt) */
[[gnu::pure]]
const std::optional<CustomGeometry> &
GetCustomGeometry() noexcept;

/**
 * The effective custom geometry for the given InfoBox panel: the
 * panel-specific one if loaded, the default one otherwise.  The
 * returned reference is stable until the next Set call, so callers
 * may compare addresses to detect whether two panels share the same
 * geometry.
 */
[[gnu::pure]]
const std::optional<CustomGeometry> &
GetCustomGeometry(unsigned panel) noexcept;

} // namespace InfoBoxLayout
