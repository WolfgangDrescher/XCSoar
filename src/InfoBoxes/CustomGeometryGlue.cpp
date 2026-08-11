// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CustomGeometryGlue.hpp"
#include "CustomGeometry.hpp"
#include "CustomGeometryFile.hpp"
#include "LogFile.hpp"
#include "Profile/Profile.hpp"
#include "system/Path.hpp"

#include <fmt/format.h>

std::string
InfoBoxLayout::MakePanelCustomGeometryFileKey(unsigned panel) noexcept
{
  return fmt::format("InfoBoxPanel{}CustomGeometryFile", panel);
}

/**
 * Load one custom geometry file into #result.  Returns false when a
 * file is configured but could not be loaded (a missing
 * configuration is not an error).
 */
static bool
LoadOne(std::string_view key,
        std::optional<InfoBoxLayout::CustomGeometry> &result) noexcept
{
  result.reset();

  const auto path = Profile::GetPath(key);
  if (path == nullptr)
    return true;

  try {
    auto geometry = InfoBoxLayout::LoadCustomGeometryFile(path);

    if (geometry.strict && geometry.screen_dpi > 0) {
      /* the layout was designed for a specific DPI setting; in
         strict mode, refuse to use it when the profile's CustomDPI
         setting differs */
      unsigned custom_dpi = 0;
      Profile::Get(ProfileKeys::CustomDPI, custom_dpi);
      if (custom_dpi != geometry.screen_dpi) {
        LogFmt("Custom InfoBox geometry '{}' requires DPI setting {}, ignoring",
               path.c_str(), geometry.screen_dpi);
        return false;
      }
    }

    result = std::move(geometry);
    return true;
  } catch (const std::exception &e) {
    LogFmt("Failed to load custom InfoBox geometry '{}': {}",
           path.c_str(), e.what());
    return false;
  }
}

bool
InfoBoxLayout::LoadCustomGeometryFromProfile() noexcept
{
  std::optional<CustomGeometry> geometry;

  bool success = LoadOne(ProfileKeys::InfoBoxCustomGeometryFile, geometry);
  SetCustomGeometry(std::move(geometry));

  for (unsigned i = 0; i < InfoBoxSettings::MAX_PANELS; ++i) {
    success &= LoadOne(MakePanelCustomGeometryFileKey(i), geometry);
    SetPanelCustomGeometry(i, std::move(geometry));
  }

  return success;
}
