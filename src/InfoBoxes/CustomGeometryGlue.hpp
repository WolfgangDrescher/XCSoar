// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string>

namespace InfoBoxLayout {

/**
 * The profile key for the custom geometry file assigned to one
 * specific InfoBox panel (set), e.g. "InfoBoxPanel3CustomGeometryFile".
 */
[[gnu::pure]]
std::string
MakePanelCustomGeometryFileKey(unsigned panel) noexcept;

/**
 * (Re)load the custom InfoBox geometry files configured in the
 * profile (ProfileKeys::InfoBoxCustomGeometryFile plus the per-panel
 * keys) into the state used by Calculate().  Call at startup and
 * whenever one of the settings changes.
 *
 * @return false if at least one configured file could not be loaded
 */
bool
LoadCustomGeometryFromProfile() noexcept;

} // namespace InfoBoxLayout
