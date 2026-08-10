// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ButtonRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Look/ButtonLook.hpp"
#include "Asset.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif

unsigned
ButtonFrameRenderer::GetMargin() noexcept
{
  return Layout::VptScale(2);
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
 * Inset the face so a soft shadow fits inside the window, and so
 * rounded corners leave the map visible in the window corners.
 */
[[gnu::pure]]
static PixelRect
GetFaceRect(PixelRect rc, ButtonState state) noexcept
{
  if (IsDithered())
    return rc;

  const unsigned pad = Layout::VptScale(2);
  const unsigned shadow = state == ButtonState::PRESSED
    ? 0
    : Layout::VptScale(3);

  rc.Grow(-(int)pad);
  if (shadow > 0) {
    /* Reserve the same strip above the face as below for the
       shadow, so pills are not tight against the action-bar top. */
    rc.top += (int)shadow;
    rc.right -= (int)shadow;
    rc.bottom -= (int)shadow;
  }

  if (state == ButtonState::PRESSED)
    rc.Offset((int)Layout::VptScale(1), (int)Layout::VptScale(1));

  return rc;
}

[[gnu::pure]]
static unsigned
GetCornerDiameter(const PixelRect &face) noexcept
{
  /* Large radius reads as a modern card / soft pill. */
  return std::min(Layout::VptScale(20),
                  std::min(std::max(2u, (unsigned)face.GetWidth() / 2),
                           std::max(2u, (unsigned)face.GetHeight() / 2)));
}

void
ButtonFrameRenderer::DrawButton(Canvas &canvas, PixelRect rc,
                                ButtonState state) const noexcept
{
  const ButtonLook::StateLook &_look = GetStateLook(look, state);
  const PixelRect face = GetFaceRect(rc, state);
  const unsigned diameter = GetCornerDiameter(face);

  Color fill = _look.background_color;
  if (state == ButtonState::PRESSED)
    fill = DarkColor(fill);

  if (IsDithered()) {
    canvas.SelectNullPen();
    Brush fill_brush{fill};
    canvas.Select(fill_brush);
    canvas.DrawRoundRectangle(face, PixelSize{diameter});
    canvas.DrawOutlineRectangle(face, COLOR_BLACK);
    return;
  }

#ifdef ENABLE_OPENGL
  {
    const ScopeAlphaBlend alpha_blend;

    if (state != ButtonState::PRESSED) {
      PixelRect shadow = face;
      shadow.Offset(Layout::VptScale(3), Layout::VptScale(3));
      canvas.SelectNullPen();
      canvas.Select(Brush{COLOR_BLACK.WithAlpha(0x55)});
      canvas.DrawRoundRectangle(shadow, PixelSize{diameter});
    }

    /* Idle buttons are slightly translucent so the map shows through. */
    if (state == ButtonState::ENABLED || state == ButtonState::DISABLED)
      fill = fill.WithAlpha(0xe6);

    canvas.SelectNullPen();
    canvas.Select(Brush{fill});
    canvas.DrawRoundRectangle(face, PixelSize{diameter});
  }
#else
  if (state != ButtonState::PRESSED) {
    PixelRect shadow = face;
    shadow.Offset(Layout::VptScale(2), Layout::VptScale(2));
    canvas.SelectNullPen();
    canvas.Select(Brush{COLOR_GRAY});
    canvas.DrawRoundRectangle(shadow, PixelSize{diameter});
  }

  canvas.SelectNullPen();
  canvas.Select(Brush{fill});
  canvas.DrawRoundRectangle(face, PixelSize{diameter});
#endif

  /* Hairline edge for definition without Win95 bevel chrome. */
  Pen edge{Layout::ScaleFinePenWidth(1), _look.dark_border_pen.GetColor()};
  canvas.Select(edge);
  canvas.SelectHollowBrush();
  canvas.DrawRoundRectangle(face, PixelSize{diameter});
}

PixelRect
ButtonFrameRenderer::GetDrawingRect(PixelRect rc, ButtonState state) const noexcept
{
  rc = GetFaceRect(rc, state);
  rc.Grow(-(int)GetMargin());
  return rc;
}

unsigned
ButtonRenderer::GetMinimumButtonWidth() const noexcept
{
  return Layout::GetMaximumControlHeight();
}
