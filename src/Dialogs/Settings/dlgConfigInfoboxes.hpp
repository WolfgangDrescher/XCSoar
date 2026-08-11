// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxes/InfoBoxSettings.hpp"

struct DialogLook;
struct InfoBoxLook;
namespace UI { class SingleWindow; }

/**
 * @param panel_index the index of the edited panel (set) in
 * InfoBoxSettings::panels; used for per-panel settings such as the
 * custom geometry file
 *
 * @return true when the #InfoBoxPanelConfig object has been modified
 */
bool
dlgConfigInfoboxesShowModal(UI::SingleWindow &parent,
                            const DialogLook &dialog_look,
                            const InfoBoxLook &look,
                            InfoBoxSettings::Geometry geometry,
                            InfoBoxSettings::Panel &data,
                            unsigned panel_index,
                            bool allow_name_change);
