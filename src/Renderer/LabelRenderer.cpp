// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LabelRenderer.hpp"
#include "LabelBlock.hpp"
#include "BoxShadowRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Pen.hpp"
#include "Math/Angle.hpp"
#include "Screen/Layout.hpp"
#include "util/UTF8.hpp"

#include <algorithm>

#include <math.h>

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#include "ui/canvas/opengl/Triangulate.hpp"
#endif

static PixelPoint
MoveInView(PixelRect &rc, const PixelRect &map_rc) noexcept
{
  PixelPoint offset(0, 0);

  // If label is above maprect
  if (map_rc.top > rc.top) {
    // Move label down into maprect
    unsigned d = map_rc.top - rc.top;
    rc.top += d;
    rc.bottom += d;
    offset.y += d;
  }

  // If label is right of maprect
  if (map_rc.right < rc.right) {
    unsigned d = map_rc.right - rc.right;
    rc.right += d;
    rc.left += d;
    offset.x += d;
  }

  // If label is below maprect
  if (map_rc.bottom < rc.bottom) {
    unsigned d = map_rc.bottom - rc.bottom;
    rc.top += d;
    rc.bottom += d;
    offset.y += d;
  }

  // If label is left of maprect
  if (map_rc.left > rc.left) {
    unsigned d = map_rc.left - rc.left;
    rc.right += d;
    rc.left += d;
    offset.x += d;
  }

  return offset;
}

/**
 * Stamp the text along a circle of the given radius, one stamp per
 * ~1.5px of circumference so consecutive stamps always overlap.  The
 * four diagonal copies this used to be only close up while the offset
 * is 1px; beyond that they leave gaps along every stroke, which is
 * what high-DPI screens hit (offset 5 on a 3x iPhone).
 */
static void
DrawTextHalo(Canvas &canvas, const char *text, const PixelPoint p,
             const unsigned offset) noexcept
{
  /* 8 is the full neighbourhood of a 1px halo, 16 caps the cost */
  const unsigned n = std::clamp(4 * offset, 8u, 16u);

  for (unsigned i = 0; i < n; ++i) {
    const auto [sin, cos] =
      (Angle::FullCircle() * ((double)i / n)).SinCos();
    canvas.DrawText({p.x + (int)lround(cos * offset),
                     p.y + (int)lround(sin * offset)},
                    text);
  }
}

void
RenderShadowedText(Canvas &canvas, const char *text,
                   PixelPoint p,
                   bool inverted) noexcept
{
  if (text == nullptr || text[0] == '\0')
    return;

  canvas.SetBackgroundTransparent();

  canvas.SetTextColor(inverted ? COLOR_BLACK : COLOR_WHITE);

  /* at least 1px, or tiny fonts get no halo at all */
  DrawTextHalo(canvas, text, p, std::max(1u, canvas.GetFontHeight() / 12u));

  canvas.SetTextColor(inverted ? COLOR_WHITE : COLOR_BLACK);
  canvas.DrawText(p, text);
}

/** The colour of a label box */
static constexpr Color BOX_COLOR = COLOR_WHITE;

/**
 * The diameter of the corners of a label box, as taken by
 * Canvas::DrawRoundRectangle(); 0 for square corners.
 */
[[gnu::pure]]
static unsigned
GetCornerDiameter(const PixelRect &rc,
                  LabelStyle::BorderRadius radius) noexcept
{
  switch (radius) {
  case LabelStyle::BorderRadius::NONE:
    break;

  case LabelStyle::BorderRadius::SMALL:
    /* capped, so short labels stay rounded rectangles instead of
       pills */
    return std::min(Layout::VptScale(8),
                    std::max(2u, (unsigned)rc.GetHeight() / 2));

  case LabelStyle::BorderRadius::FULL:
    return rc.GetHeight();
  }

  return 0;
}

/**
 * The width of a hairline border.
 */
[[gnu::const]]
static unsigned
GetBorderWidth() noexcept
{
  /* A hairline pen breaks up along the rounded corners where the
     outline is emitted as a triangle strip: its half width (0.5px)
     rounds to zero on most of the arc segments, leaving a dotted
     edge.  Widen the pen only there.  Where GL_LINE_LOOP draws the
     outline (and on the non-OpenGL canvases), a DPI-scaled pen would
     merely make the box fat and - because LineToTriangles() rounds
     the segment offsets to whole pixels - ragged around the corners;
     on a 3x iPhone it turns the 1px hairline into 3px. */
  unsigned width = 1;
#ifdef ENABLE_OPENGL
  if (!UseOpenGLLineLoopOutline(width))
    width = std::max(2u, Layout::ScaleFinePenWidth(1));
#endif
  return width;
}

/**
 * Draw the shape of a label box with the selected pen and brush.
 */
static void
DrawBoxShape(Canvas &canvas, const PixelRect &rc, unsigned diameter) noexcept
{
  if (diameter > 0)
    canvas.DrawRoundRectangle(rc, PixelSize{diameter});
  else
    canvas.DrawRectangle(rc);
}

#ifdef ENABLE_OPENGL

/**
 * Draw the shadow of a label box.  Behind a translucent box, it would
 * show through, so the box's area is masked out with the stencil
 * buffer.
 */
static void
DrawBoxShadow(Canvas &canvas, const PixelRect &rc, unsigned diameter,
              bool translucent) noexcept
{
  if (!translucent) {
    DrawBoxShadow(rc, BoxShadowStyle::FLOATING, diameter / 2);
    return;
  }

  const GLEnable<GL_STENCIL_TEST> stencil_test;
  glClear(GL_STENCIL_BUFFER_BIT);

  /* mark the box's area */
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

  canvas.SelectNullPen();
  canvas.SelectWhiteBrush();
  DrawBoxShape(canvas, rc, diameter);

  /* draw the shadow everywhere else */
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_NOTEQUAL, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  DrawBoxShadow(rc, BoxShadowStyle::FLOATING, diameter / 2);
}

#endif

/**
 * Draw the box behind the text: shadow, background and border.
 */
static void
DrawLabelBox(Canvas &canvas, const PixelRect &rc,
             const LabelStyle &style) noexcept
{
  const unsigned diameter = GetCornerDiameter(rc, style.border_radius);

  auto border = style.border;
  auto background = style.background;

#ifdef ENABLE_OPENGL
  const bool translucent =
    background == LabelStyle::Background::TRANSLUCENT;

  if (style.shadow != LabelStyle::Shadow::NONE)
    DrawBoxShadow(canvas, rc, diameter, translucent);

  const ScopeAlphaBlend alpha_blend;
#else
  /* no shadow and no blending without OpenGL: a black border
     replaces the shadow, and a translucent box is opaque */
  if (style.shadow != LabelStyle::Shadow::NONE &&
      border == LabelStyle::Border::NONE)
    border = LabelStyle::Border::BLACK;

  if (background == LabelStyle::Background::TRANSLUCENT)
    background = LabelStyle::Background::OPAQUE;
#endif

  switch (border) {
  case LabelStyle::Border::NONE:
    canvas.SelectNullPen();
    break;

  case LabelStyle::Border::BLACK:
    canvas.Select(Pen{GetBorderWidth(), COLOR_BLACK});
    break;

  case LabelStyle::Border::WHITE:
    canvas.Select(Pen{GetBorderWidth(), COLOR_WHITE});
    break;
  }

  if (background == LabelStyle::Background::NONE)
    canvas.SelectHollowBrush();
#ifdef ENABLE_OPENGL
  else if (translucent)
    canvas.Select(Brush{BOX_COLOR.WithAlpha(style.opacity)});
#endif
  else
    canvas.Select(Brush{BOX_COLOR});

  DrawBoxShape(canvas, rc, diameter);
}

/**
 * Draw the text in the given colour, with its halo.
 */
static void
DrawLabelText(Canvas &canvas, PixelPoint p, const char *text,
              LabelStyle::Text style) noexcept
{
  switch (style) {
  case LabelStyle::Text::BLACK:
    canvas.SetBackgroundTransparent();
    canvas.SetTextColor(COLOR_BLACK);
    canvas.DrawText(p, text);
    break;

  case LabelStyle::Text::BLACK_WITH_WHITE_HALO:
    RenderShadowedText(canvas, text, p, false);
    break;

  case LabelStyle::Text::WHITE_WITH_BLACK_HALO:
    RenderShadowedText(canvas, text, p, true);
    break;
  }
}

bool
LabelRenderer::Draw(Canvas &canvas, const char *text, PixelPoint anchor,
                    const LabelStyle &style, LabelPlacement placement,
                    const PixelRect &clip_rc,
                    LabelBlock *label_block) noexcept
{
  if (text == nullptr || text[0] == '\0' || !ValidateUTF8(text))
    text = "?";

  const PixelSize tsize = canvas.CalcTextSize(text);

  /* the top left corner of the text */
  PixelPoint p = anchor;

  switch (placement.horizontal_align) {
  case LabelPlacement::HorizontalAlign::LEFT:
    break;

  case LabelPlacement::HorizontalAlign::CENTER:
    p.x -= tsize.width / 2;
    break;

  case LabelPlacement::HorizontalAlign::RIGHT:
    p.x -= tsize.width;
    break;
  }

  switch (placement.vertical_align) {
  case LabelPlacement::VerticalAlign::TOP:
    break;

  case LabelPlacement::VerticalAlign::MIDDLE:
    p.y -= tsize.height / 2;
    break;

  case LabelPlacement::VerticalAlign::BOTTOM:
    p.y -= tsize.height;
    break;
  }

  const unsigned padding = Layout::GetTextPadding();

  /* the round ends of a pill need room of their own */
  const bool pill = style.HasBox() &&
    style.border_radius == LabelStyle::BorderRadius::FULL;
  const unsigned side_padding = pill
    ? (tsize.height + 2 * padding) / 2
    : padding;

  PixelRect rc;
  rc.left = p.x - side_padding - 1;
  rc.right = p.x + tsize.width + side_padding;
  rc.top = p.y - (int)padding;
  rc.bottom = p.y + tsize.height + padding;

  if (placement.move_in_view) {
    const auto offset = MoveInView(rc, clip_rc);
    p.x += offset.x;
    p.y += offset.y;
  }

  if (label_block != nullptr && !label_block->check(rc))
    return false;

  if (style.HasBox())
    DrawLabelBox(canvas, rc, style);

  DrawLabelText(canvas, p, text, style.text);
  return true;
}

bool
LabelRenderer::Draw(Canvas &canvas, const char *text, PixelPoint anchor,
                    const LabelStyle &style, LabelPlacement placement,
                    PixelSize screen_size,
                    LabelBlock *label_block) noexcept
{
  return Draw(canvas, text, anchor, style, placement,
              PixelRect{screen_size}, label_block);
}
