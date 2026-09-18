// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlassRenderer.hpp"
#include "ui/canvas/Canvas.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

#if defined(EYE_CANDY) && defined(ENABLE_OPENGL)

#include "ui/canvas/opengl/Scissor.hpp"
#include "ui/canvas/opengl/VertexPointer.hpp"
#include "util/Macros.hpp"

#include <algorithm>

/**
 * Is this a light background, on which the glass sheen shows?  The
 * alpha does not matter: a translucent box gets the sheen as well.
 */
[[gnu::const]]
static bool
IsLight(Color color) noexcept
{
  return color.Red() >= 0xf0 && color.Green() >= 0xf0 &&
    color.Blue() >= 0xf0;
}

#endif

void
DrawGlassBackground(Canvas &canvas, const PixelRect &rc, Color color) noexcept
{
#ifdef ENABLE_OPENGL
  /* the colour may be translucent */
  const ScopeAlphaBlend alpha_blend;
#endif

  canvas.DrawFilledRectangle(rc, color);

#if defined(EYE_CANDY) && defined(ENABLE_OPENGL)
  if (!IsLight(color))
    /* apply only to light backgrounds for now */
    return;

  const GLCanvasScissor scissor(rc);

  /* Shadow() keeps the alpha, so the sheen is as translucent as the
     box */
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
