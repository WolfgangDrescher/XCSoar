// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TranslucentSurface.hpp"
#include "Look/Colors.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "ui/dim/Rect.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/FrostedGlass.hpp"
#include "ui/canvas/opengl/Scope.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#endif

static bool frosted = false;

/**
 * The alpha values of the three #TranslucentSurface::Translucency
 * levels without and with frosted glass.
 */
static constexpr uint8_t ALPHA[][2] = {
  {0xf0, 0xe0}, // SUBTLE
  {0xd0, 0xb0}, // MODERATE
  {ALPHA_OVERLAY, 0x80}, // STRONG
};

void
TranslucentSurface::SetFrosted(bool enabled) noexcept
{
#ifdef ENABLE_OPENGL
  frosted = enabled;
#else
  (void)enabled;
#endif
}

bool
TranslucentSurface::IsFrosted() noexcept
{
  return frosted;
}

uint8_t
TranslucentSurface::Alpha(Translucency translucency) noexcept
{
  return ALPHA[unsigned(translucency)][frosted];
}

Color
TranslucentSurface::Translucent(Color color,
                                [[maybe_unused]] Translucency translucency)
  noexcept
{
#ifdef ENABLE_OPENGL
  return color.WithAlpha(Alpha(translucency));
#else
  return color;
#endif
}

void
TranslucentSurface::Prepare([[maybe_unused]] Canvas &canvas) noexcept
{
#ifdef ENABLE_OPENGL
  if (frosted)
    OpenGL::PrepareFrostedGlass(canvas);
#endif
}

void
TranslucentSurface::Frost([[maybe_unused]] Canvas &canvas,
                          [[maybe_unused]] const BulkPixelPoint *points,
                          [[maybe_unused]] unsigned num_points,
                          [[maybe_unused]] bool convex) noexcept
{
#ifdef ENABLE_OPENGL
  if (!frosted || num_points < 3)
    return;

  PixelRect bounds{points[0].x, points[0].y, points[0].x, points[0].y};
  for (unsigned i = 1; i < num_points; ++i) {
    bounds.left = std::min<int>(bounds.left, points[i].x);
    bounds.top = std::min<int>(bounds.top, points[i].y);
    bounds.right = std::max<int>(bounds.right, points[i].x);
    bounds.bottom = std::max<int>(bounds.bottom, points[i].y);
  }

  OpenGL::DrawFrostedGlass(canvas, bounds, points, num_points, convex);
#endif
}

void
TranslucentSurface::FrostRect(Canvas &canvas, PixelRect rc) noexcept
{
  const BulkPixelPoint pt[] = {
    rc.GetTopLeft(), rc.GetTopRight(),
    rc.GetBottomRight(), rc.GetBottomLeft(),
  };

  Frost(canvas, pt, 4, true);
}

void
TranslucentSurface::FrostRoundRect(Canvas &canvas, PixelRect rc,
                                   unsigned radius, unsigned corners) noexcept
{
  if (!frosted)
    return;

  BulkPixelPoint pt[ROUND_RECT_MAX_POINTS];
  const unsigned n = RoundRectPoints(pt, rc, radius, corners);
  Frost(canvas, pt, n, true);
}

void
TranslucentSurface::FrostCircle([[maybe_unused]] Canvas &canvas,
                                [[maybe_unused]] PixelPoint center,
                                [[maybe_unused]] unsigned radius) noexcept
{
#ifdef ENABLE_OPENGL
  if (!frosted)
    return;

  static constexpr unsigned SEGMENTS = 64;
  BulkPixelPoint pt[SEGMENTS];
  for (unsigned i = 0; i < SEGMENTS; ++i) {
    const float angle = float(2 * std::numbers::pi) * i / SEGMENTS;
    pt[i] = {center.x + int(std::lround(radius * std::cos(angle))),
             center.y + int(std::lround(radius * std::sin(angle)))};
  }

  Frost(canvas, pt, SEGMENTS, true);
#endif
}

void
TranslucentSurface::FillRoundRect(Canvas &canvas, PixelRect rc,
                                  unsigned radius, Color color,
                                  Translucency translucency,
                                  unsigned corners) noexcept
{
  FrostRoundRect(canvas, rc, radius, corners);

#ifdef ENABLE_OPENGL
  const ScopeAlphaBlend alpha_blend;
#endif

  canvas.Select(Brush(Translucent(color, translucency)));
  RoundRect(canvas, rc, radius, corners);
}

void
TranslucentSurface::FillCircle(Canvas &canvas, PixelPoint center,
                               unsigned radius, Color color,
                               Translucency translucency) noexcept
{
  FrostCircle(canvas, center, radius);

#ifdef ENABLE_OPENGL
  const ScopeAlphaBlend alpha_blend;
#endif

  canvas.Select(Brush(Translucent(color, translucency)));
  canvas.DrawCircle(center, radius);
}

void
TranslucentSurface::Hold([[maybe_unused]] const MapMotion &motion) noexcept
{
#ifdef ENABLE_OPENGL
  const OpenGL::FrostedGlassMotion m{motion.center, motion.offset,
                                     motion.zoom};
  OpenGL::HoldFrostedGlass(&m);
#endif
}

void
TranslucentSurface::Release() noexcept
{
#ifdef ENABLE_OPENGL
  OpenGL::HoldFrostedGlass(nullptr);
#endif
}

void
TranslucentSurface::MapPainted([[maybe_unused]] unsigned generation,
                               [[maybe_unused]] PixelPoint origin) noexcept
{
#ifdef ENABLE_OPENGL
  OpenGL::FrostedGlassMapPainted(generation, origin);
#endif
}
