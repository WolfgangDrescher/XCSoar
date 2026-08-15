// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ButtonRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Look/ButtonLook.hpp"
#include "Asset.hpp"

unsigned
ButtonFrameRenderer::GetMargin() noexcept
{
  /* wide enough for the keyboard focus ring around the face */
  return Layout::VptScale(3);
}

static constexpr const auto &
GetStateLook(const ButtonLook &look, ButtonState state) noexcept
{
  switch (state) {
  case ButtonState::DISABLED:
  case ButtonState::ENABLED:
    break;

  case ButtonState::SELECTED:
    return look.selected;

  case ButtonState::FOCUSED:
  case ButtonState::PRESSED:
    return look.focused;
  }

  return look.standard;
}

/**
 * Inset the face from the window edges so adjacent buttons get
 * breathing room and rounded corners reveal the background.
 */
[[gnu::pure]]
static PixelRect
GetFaceRect(PixelRect rc) noexcept
{
  if (!IsDithered())
    rc.Grow(-(int)ButtonFrameRenderer::GetMargin());

  return rc;
}

[[gnu::pure]]
static unsigned
GetCornerDiameter(const PixelRect &face) noexcept
{
  /* radius comparable to a Tailwind "rounded-lg" card; the cap keeps
     small buttons from turning into pills */
  return std::min(Layout::VptScale(14),
                  std::min(std::max(2u, (unsigned)face.GetWidth() / 2),
                           std::max(2u, (unsigned)face.GetHeight() / 2)));
}

void
ButtonFrameRenderer::DrawButton(Canvas &canvas, PixelRect rc,
                                ButtonState state) const noexcept
{
  const ButtonLook::StateLook &_look = GetStateLook(look, state);
  const PixelRect face = GetFaceRect(rc);
  const unsigned diameter = GetCornerDiameter(face);

  const Color fill = state == ButtonState::PRESSED
    ? _look.pressed_background_color
    : _look.background_color;

  canvas.SelectNullPen();

  if (IsDithered()) {
    const Brush fill_brush{fill};
    canvas.Select(fill_brush);
    canvas.DrawRoundRectangle(face, PixelSize{diameter});
    canvas.DrawOutlineRectangle(face, COLOR_BLACK);
    canvas.SelectHollowBrush();
    return;
  }

  if (state == ButtonState::FOCUSED || state == ButtonState::SELECTED) {
    /* a solid ring hugging the face from the outside, like a
       Tailwind `ring-3`: the light primary for keyboard focus, a
       dark one for the selected button, which shares its face
       color with the focused one.  Drawn as a filled round
       rectangle underneath the face because a filled fan
       rasterizes cleaner than a thick stroked outline */
    const unsigned width = std::max(2u, Layout::VptScale(3));
    PixelRect ring_rc = face;
    ring_rc.Grow((int)width);

    const Brush ring_brush{state == ButtonState::FOCUSED
        ? look.focus_ring_color
        : look.selected_ring_color};
    canvas.Select(ring_brush);
    canvas.DrawRoundRectangle(ring_rc, PixelSize{diameter + 2 * width});

    /* ring_brush dies at the end of this block; it must not stay
       selected (GDI) */
    canvas.SelectHollowBrush();
  }

  {
    const Brush fill_brush{fill};
    canvas.Select(fill_brush);
    canvas.DrawRoundRectangle(face, PixelSize{diameter});
    canvas.SelectHollowBrush();
  }

  /* no drop shadow: definition comes from a hairline ring on the face
     outline, like a Tailwind `ring ring-inset` */
  const Pen ring_pen{Layout::ScaleFinePenWidth(1), _look.ring_color};
  canvas.Select(ring_pen);
  canvas.DrawRoundRectangle(face, PixelSize{diameter});

  /* deselect the local pen/brush before they go out of scope (the GDI
     backend must not delete objects still selected in the DC) */
  canvas.SelectNullPen();
}

PixelRect
ButtonFrameRenderer::GetDrawingRect(PixelRect rc,
                                    [[maybe_unused]] ButtonState state) const noexcept
{
  rc = GetFaceRect(rc);
  rc.Grow(-(int)GetMargin());
  return rc;
}

unsigned
ButtonRenderer::GetMinimumButtonWidth() const noexcept
{
  return Layout::GetMaximumControlHeight();
}
