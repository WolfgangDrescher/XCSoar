// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CustomGeometry.hpp"

#include <cassert>

namespace InfoBoxLayout {

static std::optional<CustomGeometry> default_custom_geometry;
static std::optional<CustomGeometry>
  panel_custom_geometry[InfoBoxSettings::MAX_PANELS];

void
SetCustomGeometry(std::optional<CustomGeometry> geometry) noexcept
{
  default_custom_geometry = std::move(geometry);
}

void
SetPanelCustomGeometry(unsigned panel,
                       std::optional<CustomGeometry> geometry) noexcept
{
  assert(panel < InfoBoxSettings::MAX_PANELS);

  panel_custom_geometry[panel] = std::move(geometry);
}

const std::optional<CustomGeometry> &
GetCustomGeometry() noexcept
{
  return default_custom_geometry;
}

const std::optional<CustomGeometry> &
GetCustomGeometry(unsigned panel) noexcept
{
  if (panel < InfoBoxSettings::MAX_PANELS && panel_custom_geometry[panel])
    return panel_custom_geometry[panel];

  return default_custom_geometry;
}

} // namespace InfoBoxLayout
