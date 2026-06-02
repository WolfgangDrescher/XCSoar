// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlassRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Pen.hpp"
#include "ui/canvas/Util.hpp"
#include "ui/canvas/Color.hpp"
#include "Screen/Layout.hpp"

#if defined(EYE_CANDY) && defined(ENABLE_OPENGL)

#include "ui/canvas/opengl/Scissor.hpp"
#include "ui/canvas/opengl/VertexPointer.hpp"
#include "util/Macros.hpp"

#include <algorithm>

#endif

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

void
DrawGlassBackground(Canvas &canvas, const PixelRect &rc, Color color) noexcept
{
  canvas.DrawFilledRectangle(rc, color);

#if defined(EYE_CANDY) && defined(ENABLE_OPENGL)
  if (color != COLOR_WHITE)
    /* apply only to white background for now */
    return;

  const GLCanvasScissor scissor(rc);

  const Color shadow = color.Shadow();

  const auto center = rc.GetCenter();
  const int size = std::min(rc.GetWidth(), rc.GetHeight()) / 4;

  const BulkPixelPoint vertices[] = {
    center.At(1024, -1024),
    center.At(1024 + size, -1024 + size),
    center.At(-1024, 1024),
    center.At(-1024 + size, 1024 + size),
  };

  const ScopeVertexPointer vp(vertices);

  const Color colors[] = {
    shadow, color,
    shadow, color,
  };

  const ScopeColorPointer cp(colors);

  static_assert(ARRAY_SIZE(vertices) == ARRAY_SIZE(colors),
                "Array size mismatch");

  glDrawArrays(GL_TRIANGLE_STRIP, 0, ARRAY_SIZE(vertices));
#endif
}

void
DrawRoundedDarkBackground(Canvas &canvas, const PixelRect &rc,
                          Color bg_color) noexcept
{
  const int margin = Layout::Scale(2);
  PixelRect inner = rc;
  inner.left += margin;
  inner.right -= margin;
  inner.top += margin;
  inner.bottom -= margin;

  const unsigned radius = Layout::Scale(4);

#ifdef ENABLE_OPENGL
  const ScopeAlphaBlend alpha_blend;
  canvas.Select(Brush(bg_color));
  canvas.Select(Pen(0, COLOR_BLACK));
  RoundRect(canvas, inner, radius);
#else
  canvas.Select(Brush(bg_color));
  canvas.Select(Pen(0, COLOR_BLACK));
  RoundRect(canvas, inner, radius);
#endif
}
