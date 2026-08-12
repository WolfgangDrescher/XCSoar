// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxes/Content/Base.hpp"

void
UpdateInfoBoxHeartRate(InfoBoxData &data) noexcept;

void
UpdateInfoBoxGLoad(InfoBoxData &data) noexcept;

void
UpdateInfoBoxBattery(InfoBoxData &data) noexcept;

void
UpdateInfoBoxExperimental1(InfoBoxData &data) noexcept;

void
UpdateInfoBoxExperimental2(InfoBoxData &data) noexcept;

void
UpdateInfoBoxCPULoad(InfoBoxData &data) noexcept;

void
UpdateInfoBoxFreeRAM(InfoBoxData &data) noexcept;

void
UpdateInfoBoxNbrSat(InfoBoxData &data) noexcept;

void
UpdateInfoBoxSpacer(InfoBoxData &data) noexcept;

/**
 * The "invisible" InfoBox: it has no title, no value and no comment.
 * Its window is never shown; the map is extended over the slot
 * instead (see #InfoBoxManager::ExpandOverInvisible()).
 */
void
UpdateInfoBoxInvisible(InfoBoxData &data) noexcept;

class InfoBoxContentNbrSat final : public InfoBoxContent {
public:
  void Update(InfoBoxData &data) noexcept override;
  bool HandleClick() noexcept override;
};

class InfoBoxContentHorizon : public InfoBoxContent
{
public:
  void Update(InfoBoxData &data) noexcept override;
  void OnCustomPaint(Canvas &canvas, const PixelRect &rc) noexcept override;
};
