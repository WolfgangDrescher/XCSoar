// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxBackground.hpp"
#include "BoxShadowRenderer.hpp"
#include "InfoBoxes/Border.hpp"
#include "Screen/Layout.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Util.hpp"

#include <algorithm>

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#include "ui/canvas/opengl/FrostedGlass.hpp"
#include "ui/dim/BulkPoint.hpp"
#endif

static constexpr unsigned ALL_EDGES =
  BORDERTOP | BORDERRIGHT | BORDERBOTTOM | BORDERLEFT;

InfoBoxBackgroundShape
InfoBoxBackgroundShape::For(InfoBoxSettings::BorderStyle style,
                            unsigned outer_edges) noexcept
{
  switch (style) {
  case InfoBoxSettings::BorderStyle::FLOATING:
    /* every box is set back a little from all its neighbours; at the
       outer edges it takes over the neighbour's share of the gap */
    return {unsigned(Layout::Scale(2)), unsigned(Layout::Scale(4)),
            unsigned(Layout::Scale(4)), ALL_EDGES, outer_edges};

  case InfoBoxSettings::BorderStyle::DOCK:
    /* adjacent boxes join seamlessly; the block they form is set
       back from the screen edge and from the map.  The halo marks
       the panel's edge; set the last field to true for an outline
       as well. */
    return {unsigned(Layout::Scale(4)), unsigned(Layout::Scale(4)),
            unsigned(Layout::Scale(6)), outer_edges, outer_edges, false};

  case InfoBoxSettings::BorderStyle::BOX:
  case InfoBoxSettings::BorderStyle::TAB:
  case InfoBoxSettings::BorderStyle::SHADED:
  case InfoBoxSettings::BorderStyle::GLASS:
    break;
  }

  return {};
}

PixelRect
InfoBoxBackgroundShape::Inset(PixelRect rc) const noexcept
{
  const auto inset = [this](unsigned edge) -> int {
    if ((edges & edge) == 0)
      return 0;

    return int((outer_edges & edge) ? outer_margin : margin);
  };

  rc.left += inset(BORDERLEFT);
  rc.top += inset(BORDERTOP);
  rc.right -= inset(BORDERRIGHT);
  rc.bottom -= inset(BORDERBOTTOM);

  return rc;
}

Color
GetTranslucentInfoBoxColor(bool inverse,
                           InfoBoxSettings::Background t) noexcept
{
  return MakeTranslucent(inverse
                         ? Color(0x28, 0x28, 0x28)
                         : Color(0xf8, 0xf8, 0xf8),
                         t);
}

Color
GetTranslucentCaptionColor([[maybe_unused]] bool inverse,
                           [[maybe_unused]] Color solid) noexcept
{
#ifdef ENABLE_OPENGL
  return (inverse ? COLOR_WHITE : COLOR_BLACK).WithAlpha(0x20);
#else
  return solid;
#endif
}

Color
MakeTranslucent([[maybe_unused]] Color color,
                [[maybe_unused]] InfoBoxSettings::Background t) noexcept
{
#ifdef ENABLE_OPENGL
  switch (t) {
  case InfoBoxSettings::Background::SOLID:
    break;

  case InfoBoxSettings::Background::TRANSPARENT:
    /* let the map shine through a little */
    return color.WithAlpha(0xf0);

  case InfoBoxSettings::Background::FROSTED:
    /* 88 percent: the blur keeps the text readable, so a bit more of
       the map may show than without */
    return color.WithAlpha(0xe0);
  }
#endif

  return color;
}

/**
 * The corners where two of the shape's edges meet, as RoundRect()
 * flags.
 */
[[gnu::pure]]
static unsigned
GetRoundedCorners(const InfoBoxBackgroundShape &shape) noexcept
{
  unsigned corners = 0;
  if ((shape.edges & (BORDERTOP | BORDERLEFT)) == (BORDERTOP | BORDERLEFT))
    corners |= ROUND_TOP_LEFT;
  if ((shape.edges & (BORDERTOP | BORDERRIGHT)) ==
      (BORDERTOP | BORDERRIGHT))
    corners |= ROUND_TOP_RIGHT;
  if ((shape.edges & (BORDERBOTTOM | BORDERRIGHT)) ==
      (BORDERBOTTOM | BORDERRIGHT))
    corners |= ROUND_BOTTOM_RIGHT;
  if ((shape.edges & (BORDERBOTTOM | BORDERLEFT)) ==
      (BORDERBOTTOM | BORDERLEFT))
    corners |= ROUND_BOTTOM_LEFT;
  return corners;
}

/**
 * Translate BORDER* flags to RoundRectOutline() edge flags.
 */
[[gnu::const]]
static unsigned
ToRoundRectEdges(unsigned border_edges) noexcept
{
  unsigned edges = 0;
  if (border_edges & BORDERTOP)
    edges |= ROUND_RECT_TOP;
  if (border_edges & BORDERRIGHT)
    edges |= ROUND_RECT_RIGHT;
  if (border_edges & BORDERBOTTOM)
    edges |= ROUND_RECT_BOTTOM;
  if (border_edges & BORDERLEFT)
    edges |= ROUND_RECT_LEFT;
  return edges;
}

void
DrawInfoBoxBackground(Canvas &canvas, PixelRect rc,
                      const InfoBoxBackgroundShape &shape,
                      Color color) noexcept
{
  rc = shape.Inset(rc);

  const unsigned corners = GetRoundedCorners(shape);

#ifdef ENABLE_OPENGL
  const ScopeAlphaBlend alpha_blend;
#endif

  if (corners == 0 || shape.radius == 0) {
    canvas.DrawFilledRectangle(rc, color);
    return;
  }

  canvas.SelectNullPen();
  canvas.Select(Brush(color));
  RoundRect(canvas, rc, shape.radius, corners);
}

void
DrawInfoBoxHalo([[maybe_unused]] Canvas &canvas, PixelRect rc,
                const InfoBoxBackgroundShape &shape) noexcept
{
  if (shape.edges == 0 || shape.margin == 0)
    return;

  const unsigned blur = 2 * std::max(shape.margin, shape.outer_margin);

  /* where the box joins a neighbour, push its edge out of the window
     so the halo runs straight along the seam instead of rounding a
     corner there */
  PixelRect box = shape.Inset(rc);
  if ((shape.edges & BORDERLEFT) == 0)
    box.left -= int(blur);
  if ((shape.edges & BORDERTOP) == 0)
    box.top -= int(blur);
  if ((shape.edges & BORDERRIGHT) == 0)
    box.right += int(blur);
  if ((shape.edges & BORDERBOTTOM) == 0)
    box.bottom += int(blur);

  /* half of this at the box edge, fading out towards the window
     edge; the dialogs use a stronger shadow, see
     Renderer/BoxShadowRenderer */
  static constexpr uint8_t HALO_ALPHA = 80;

  DrawBoxHalo(rc, box, shape.radius, blur, HALO_ALPHA);
}

void
DrawInfoBoxFrostedGlass([[maybe_unused]] Canvas &canvas,
                        [[maybe_unused]] PixelRect rc,
                        [[maybe_unused]] const InfoBoxBackgroundShape &shape)
  noexcept
{
#ifdef ENABLE_OPENGL
  rc = shape.Inset(rc);

  BulkPixelPoint pt[ROUND_RECT_MAX_POINTS];
  const unsigned n = RoundRectPoints(pt, rc, shape.radius,
                                     GetRoundedCorners(shape));
  OpenGL::DrawFrostedGlass(canvas, rc, pt, n);
#endif
}

Color
GetInfoBoxOutlineColor() noexcept
{
#ifdef ENABLE_OPENGL
  return COLOR_GRAY.WithAlpha(0x60);
#else
  return COLOR_GRAY;
#endif
}

void
DrawInfoBoxSeparators(Canvas &canvas, PixelRect rc,
                      const InfoBoxBackgroundShape &shape,
                      unsigned edges, const Pen &pen) noexcept
{
  if (edges == 0)
    return;

  const PixelRect box = shape.Inset(rc);

#ifdef ENABLE_OPENGL
  /* the pen may be translucent */
  const ScopeAlphaBlend alpha_blend;
#endif

  canvas.Select(pen);

  if (edges & BORDERTOP)
    canvas.DrawExactLine({box.left, box.top}, {box.right - 1, box.top});

  if (edges & BORDERRIGHT)
    canvas.DrawExactLine({box.right - 1, box.top},
                         {box.right - 1, box.bottom});

  if (edges & BORDERBOTTOM)
    canvas.DrawExactLine({box.left, box.bottom - 1},
                         {box.right - 1, box.bottom - 1});

  if (edges & BORDERLEFT)
    canvas.DrawExactLine({box.left, box.top}, {box.left, box.bottom - 1});
}

void
DrawInfoBoxOutline(Canvas &canvas, PixelRect rc,
                   const InfoBoxBackgroundShape &shape,
                   const Pen &pen) noexcept
{
  if (!shape.outline || shape.edges == 0)
    return;

  /* the fill covers the pixels up to right/bottom exclusively; run the
     outline over its outermost pixels, not beside them */
  rc = shape.Inset(rc);
  --rc.right;
  --rc.bottom;

#ifdef ENABLE_OPENGL
  /* the pen may be translucent */
  const ScopeAlphaBlend alpha_blend;
#endif

  canvas.Select(pen);
  canvas.SelectHollowBrush();
  RoundRectOutline(canvas, rc, shape.radius, GetRoundedCorners(shape),
                   ToRoundRectEdges(shape.edges));
}
