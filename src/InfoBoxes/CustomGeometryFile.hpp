// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "CustomGeometry.hpp"

class Path;

namespace InfoBoxLayout {

/**
 * Load and parse a custom InfoBox geometry JSON file.
 *
 * Throws on I/O or parser error.
 */
CustomGeometry
LoadCustomGeometryFile(Path path);

} // namespace InfoBoxLayout
