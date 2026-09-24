// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Math/Point2D.hpp"

#include <span>
#include <vector>

class Color;
struct BulkPixelPoint;

/**
 * Lines with round ends and joins and a smooth, optionally wide and
 * soft edge, drawn with #OpenGL::round_line_shader.  Unlike a wide
 * polyline, the joins stay round even where a line turns back on
 * itself.  Each segment may have its own radius; a segment whose ends
 * are the same point is a dot.
 *
 * Collect the segments, then call Draw() for each colour; a soft
 * shadow is a second #RoundLines with a wide soft edge.
 */
class RoundLines {
  struct Vertex {
    FloatPoint2D position;

    /** the end points of the segment this vertex belongs to */
    FloatPoint2D a, b;

    float radius;
  };

  std::vector<Vertex> vertices;

  /** the width of the soft edge, centred on the radius */
  float softness;

public:
  /**
   * @param _softness the width of the soft edge in pixels; 1 is a
   * crisp, anti-aliased edge
   */
  explicit RoundLines(float _softness=1) noexcept
    :softness(_softness) {}

  bool empty() const noexcept {
    return vertices.empty();
  }

  void clear() noexcept {
    vertices.clear();
  }

  void AddSegment(FloatPoint2D a, FloatPoint2D b, float radius) noexcept;

  void AddDot(FloatPoint2D center, float radius) noexcept {
    AddSegment(center, center, radius);
  }

  /**
   * Add a polyline.
   */
  void AddLine(std::span<const BulkPixelPoint> points,
               float radius) noexcept;

  /**
   * Draw all segments.  A translucent colour is blended only once
   * where segments overlap, so their joints do not show.
   */
  void Draw(Color color) const noexcept;
};
