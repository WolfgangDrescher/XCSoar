// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxSettings.hpp"
#include "ui/dim/Rect.hpp"

namespace InfoBoxLayout {

struct Layout {
  InfoBoxSettings::Geometry geometry;

  bool landscape;

  PixelSize control_size;

  unsigned count;
  PixelRect positions[InfoBoxSettings::Panel::MAX_CONTENTS];

  /**
   * Per-box border flags (bitmask of BORDERTOP etc.); only valid
   * when #geometry is InfoBoxSettings::Geometry::CUSTOM (built-in
   * geometries derive their borders from GetBorder()).
   */
  unsigned char custom_borders[InfoBoxSettings::Panel::MAX_CONTENTS];

  PixelRect vario;

  PixelRect remaining;

  constexpr bool HasVario() const noexcept {
    return vario.right > vario.left && vario.bottom > vario.top;
  }

  void ClearVario() noexcept {
    vario.left = vario.top = vario.right = vario.bottom = 0;
  }
};

/**
 * Calculate the layout using the default custom geometry when
 * #geometry is InfoBoxSettings::Geometry::CUSTOM.
 */
[[gnu::pure]]
Layout
Calculate(PixelRect rc, InfoBoxSettings::Geometry geometry) noexcept;

/**
 * Like Calculate(), but for InfoBoxSettings::Geometry::CUSTOM the
 * custom geometry assigned to the given InfoBox panel (set) is
 * preferred over the default one.
 */
[[gnu::pure]]
Layout
Calculate(PixelRect rc, InfoBoxSettings::Geometry geometry,
          unsigned panel_index) noexcept;

/**
 * The border flags for a built-in geometry.  Not applicable to
 * Geometry::CUSTOM, whose borders are in Layout::custom_borders.
 */
[[gnu::const]]
int
GetBorder(InfoBoxSettings::Geometry geometry, bool landscape,
          unsigned i) noexcept;

} // namespace InfoBoxLayout
