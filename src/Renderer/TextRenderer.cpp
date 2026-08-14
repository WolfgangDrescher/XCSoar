// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TextRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/AnyCanvas.hpp"
#include "ui/canvas/TextFormat.hpp"
#include "Asset.hpp"

#include <winuser.h>

unsigned
TextRenderer::GetHeight(Canvas &canvas, PixelRect rc,
                        std::string_view text) const noexcept
{
  return canvas.DrawFormattedText(rc, text, DT_CALCRECT);
}

unsigned
TextRenderer::GetHeight(Canvas &canvas, unsigned width,
                        std::string_view text) const noexcept
{
  return GetHeight(canvas, PixelRect(0, 0, width, 0), text);
}

unsigned
TextRenderer::GetHeight(const Font &font, unsigned width,
                        std::string_view text) const noexcept
{
  AnyCanvas canvas;
  canvas.Select(font);
  return GetHeight(canvas, width, text);
}

void
TextRenderer::Draw(Canvas &canvas, PixelRect rc,
                   std::string_view text) const noexcept
{
  unsigned format = (center ? DT_CENTER : DT_LEFT);

#ifdef USE_GDI
  if (vcenter) {
    const unsigned height = GetHeight(canvas, rc, text);
    TEXTMETRIC tm;
    unsigned block = height;
    /* Single-line captions: center the cap-height strip (cap height
       is roughly tmAscent - tmInternalLeading, so the block
       2*ascent - cap simplifies to ascent + internal leading), then
       lift the ink slightly above the geometric middle like native
       buttons do; exact centering reads as sitting too low. */
    if (::GetTextMetrics(canvas, &tm) &&
        height <= (unsigned)tm.tmHeight)
      block = tm.tmAscent + tm.tmInternalLeading
        + 2 * (3 * (unsigned)tm.tmDescent / 8);
    int top = (rc.top + rc.bottom - (int)block) / 2;
    if (top > rc.top)
      rc.top = top;
  }
#else
  if (vcenter)
    format |= DT_VCENTER;

  if (control && IsDithered())
    /* button texts are underlined on the Kobo */
    format |= DT_UNDERLINE;
#endif

  canvas.DrawFormattedText(rc, text, format);
}
