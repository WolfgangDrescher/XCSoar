// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GestureRenderer.hpp"
#include "Look/GestureLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Math/FastMath.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * Minimum extent of the trail in pixels.  Without it, a tap would
 * flash the dot which marks the pointer position.
 */
static constexpr int MIN_TRAIL_EXTENT = 2;

/**
 * Number of jitter damping passes applied before and after
 * resampling.
 */
static constexpr unsigned DAMP_PASSES = 2;

/**
 * The distance between the points the trail is drawn from.  Short
 * segments would make the direction of each segment depend on the
 * pixel rounding of its ends, and a wide line drawn through them
 * looks frayed.
 */
[[gnu::const]]
static unsigned
GetTrailSpacing(unsigned width) noexcept
{
  return std::max(6u, width);
}

/**
 * Has the pointer moved far enough to draw a trail?
 */
[[gnu::pure]]
static bool
HasVisibleExtent(std::span<const PixelPoint> points) noexcept
{
  const PixelPoint first = points.front();

  for (const auto &p : points) {
    const auto d = p - first;
    if (compare_squared(d.x, d.y, MIN_TRAIL_EXTENT) == 1)
      return true;
  }

  return false;
}

/**
 * Damp the jitter of the touch screen; the first and the last point
 * are kept, so the trail still ends exactly at the finger.
 */
static void
DampJitter(std::span<const PixelPoint> src,
           std::vector<PixelPoint> &dest) noexcept
{
  dest.clear();
  dest.reserve(src.size());

  dest.push_back(src.front());

  for (std::size_t i = 1; i + 1 < src.size(); ++i)
    dest.emplace_back((src[i - 1].x + 2 * src[i].x + src[i + 1].x) / 4,
                      (src[i - 1].y + 2 * src[i].y + src[i + 1].y) / 4);

  dest.push_back(src.back());
}

/**
 * Distribute points evenly along the recorded path.
 */
static void
Resample(std::span<const PixelPoint> src, unsigned spacing,
         std::vector<PixelPoint> &dest) noexcept
{
  dest.clear();
  dest.reserve(src.size());

  dest.push_back(src.front());

  double carry = 0;

  for (std::size_t i = 0; i + 1 < src.size(); ++i) {
    const PixelPoint a = src[i];
    const auto d = src[i + 1] - a;
    const double length = std::hypot(double(d.x), double(d.y));
    if (length <= 0)
      continue;

    double position = spacing - carry;
    for (; position <= length; position += spacing) {
      const double t = position / length;
      dest.emplace_back(a.x + int(d.x * t), a.y + int(d.y * t));
    }

    carry = length - (position - spacing);
  }

  if (dest.back() != src.back())
    dest.push_back(src.back());
}

/**
 * Convert the recorded points to an evenly spaced smooth line.
 */
static void
BuildTrail(std::span<const PixelPoint> src, unsigned spacing,
           std::vector<BulkPixelPoint> &dest) noexcept
{
  std::vector<PixelPoint> a, b;

  DampJitter(src, a);
  for (unsigned i = 1; i < DAMP_PASSES; ++i) {
    DampJitter(a, b);
    a.swap(b);
  }

  Resample(a, spacing, b);

  for (unsigned i = 0; i < DAMP_PASSES; ++i) {
    DampJitter(b, a);
    a.swap(b);
  }

  dest.assign(b.begin(), b.end());
}

#ifdef ENABLE_OPENGL

/**
 * How much of the line has to be cut off so that its end cap becomes
 * a chord of the dot drawn at the pointer position: both corners of
 * the cap then sit exactly on the dot's edge.
 */
[[gnu::pure]]
static double
GetTipTrimDistance(const GestureLook &look) noexcept
{
  const double radius = look.tip_radius;
  const double half_width = look.width / 2.;

  return radius > half_width
    ? std::sqrt(radius * radius - half_width * half_width)
    : 0;
}

/**
 * Cut the given distance off the end of the line.
 */
static void
TrimTail(std::vector<BulkPixelPoint> &line, double distance) noexcept
{
  while (line.size() >= 2 && distance > 0) {
    const PixelPoint last = line.back();
    const PixelPoint previous = line[line.size() - 2];
    const double dx = last.x - previous.x;
    const double dy = last.y - previous.y;
    const double length = std::hypot(dx, dy);

    if (length > distance) {
      const double t = 1 - distance / length;
      line.back() = PixelPoint(previous.x + int(dx * t),
                               previous.y + int(dy * t));
      return;
    }

    distance -= length;
    line.pop_back();
  }
}

#endif /* ENABLE_OPENGL */

void
GestureRenderer::Draw(Canvas &canvas, const GestureLook &look,
                      std::span<const PixelPoint> points,
                      bool valid) noexcept
{
  if (points.size() < 2 || !HasVisibleExtent(points))
    return;

  std::vector<BulkPixelPoint> line;
  BuildTrail(points, GetTrailSpacing(look.width), line);
  if (line.size() < 2)
    return;

  canvas.SelectHollowBrush();

#ifdef ENABLE_OPENGL
  /* the line ends flush with the dot below the pointer */
  TrimTail(line, GetTipTrimDistance(look));
  if (line.size() < 2)
    return;
#endif

  canvas.Select(valid ? look.pen : look.invalid_pen);
  canvas.DrawPolyline(line.data(), unsigned(line.size()));

#ifdef ENABLE_OPENGL
  /* a dot marks the current pointer position */
  const Color color = valid ? look.color : look.invalid_color;
  canvas.Select(Pen(1, color));
  canvas.Select(Brush(color));
  canvas.DrawCircle(points.back(), look.tip_radius);
#endif
}
