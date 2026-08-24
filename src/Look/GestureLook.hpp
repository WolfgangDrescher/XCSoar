// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Color.hpp"
#include "ui/canvas/Pen.hpp"

struct GestureLook
{
  /** Colour of a recognised gesture */
  Color color;

  /** Colour of a gesture which is not (yet) recognised */
  Color invalid_color;

  /** Width of the core line */
  unsigned width;

  /** Radius of the dot drawn at the current pointer position */
  unsigned tip_radius;

  Pen pen, invalid_pen;

  void Initialise();
};
