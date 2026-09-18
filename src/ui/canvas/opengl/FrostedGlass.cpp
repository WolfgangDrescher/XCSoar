// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FrostedGlass.hpp"
#include "Globals.hpp"
#include "Texture.hpp"
#include "Shaders.hpp"
#include "Program.hpp"
#include "VertexPointer.hpp"
#include "ui/canvas/BufferCanvas.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Math/Point2D.hpp"

#include <algorithm>
#include <vector>

/**
 * The off-screen passes run at this fraction of the screen
 * resolution.  Shrinking with the linear texture filter is the first
 * blur step, and it makes the Gaussian passes cheap and wide.
 */
static constexpr unsigned DOWNSAMPLE = 4;

namespace {

struct FrostedGlassResources {
  /**
   * The copy of the whole framebuffer.  Copied from the framebuffer,
   * its rows are stored bottom-up, hence "flipped".
   */
  GLTexture source{GL_RGB, PixelSize{DOWNSAMPLE, DOWNSAMPLE},
                   GL_RGB, GL_UNSIGNED_BYTE, true};

  /**
   * Two small buffers the blur passes ping-pong between; pass[0]
   * holds the result.
   */
  BufferCanvas pass[2];

  /**
   * The frame the result was made in, see OpenGL::frame_serial.
   */
  unsigned frame_serial;

  bool valid = false;

  /**
   * The screen size the result was made for.
   */
  PixelSize screen_size;

  /**
   * The size of the result within pass[0].
   */
  PixelSize small_size;
};

} // anonymous namespace

static FrostedGlassResources *resources = nullptr;

/**
 * One blur pass: draw @p src into @p dest with #OpenGL::blur_shader.
 */
static void
BlurPass(BufferCanvas &dest, const BufferCanvas &src,
         FloatPoint2D step) noexcept
{
  GLTexture &texture = src.GetTexture();

  dest.Begin();

  OpenGL::blur_shader->Use();
  glUniform2f(OpenGL::blur_step, step.x, step.y);

  texture.Bind();
  texture.Draw(PixelRect{dest.GetSize()}, PixelRect{src.GetSize()});

  dest.End();
}

/**
 * Make sure the blurred copy of the framebuffer is up to date for
 * this frame.
 */
static FrostedGlassResources &
Prepare(Canvas &canvas) noexcept
{
  if (resources == nullptr)
    resources = new FrostedGlassResources();

  auto &r = *resources;

  const PixelSize screen_size{OpenGL::viewport_size.x,
                              OpenGL::viewport_size.y};

  if (r.valid && r.frame_serial == OpenGL::frame_serial &&
      r.screen_size == screen_size)
    return r;

  r.valid = false;
  r.frame_serial = OpenGL::frame_serial;
  r.screen_size = screen_size;
  r.small_size = {
    std::max(1u, screen_size.width / DOWNSAMPLE),
    std::max(1u, screen_size.height / DOWNSAMPLE),
  };

  if (r.source.GetSize() != screen_size)
    r.source.ResizeDiscard(GL_RGB, screen_size, GL_RGB, GL_UNSIGNED_BYTE);

  for (auto &pass : r.pass) {
    if (!pass.IsDefined())
      pass.Create(r.small_size);
    else
      pass.Resize(r.small_size);
  }

  /* 1. grab the whole screen; the canvas may be a SubCanvas, so
     address the screen relative to its origin */
  const PixelRect screen_rc{
    PixelPoint{-OpenGL::translate.x, -OpenGL::translate.y},
    screen_size,
  };
  canvas.CopyToTexture(r.source, screen_rc);

  /* 2. shrink it */
  r.pass[0].Begin();
  OpenGL::texture_shader->Use();
  r.source.Bind();
  r.source.Draw(PixelRect{r.small_size}, PixelRect{screen_size});
  r.pass[0].End();

  /* 3. blur it, horizontally and then vertically; the taps are
     spaced in texture coordinates, which span the allocated (maybe
     larger, power-of-two) texture */
  const PixelSize allocated = r.pass[0].GetTexture().GetAllocatedSize();
  BlurPass(r.pass[1], r.pass[0], {1.f / allocated.width, 0.f});
  BlurPass(r.pass[0], r.pass[1], {0.f, 1.f / allocated.height});

  r.valid = true;
  return r;
}

void
OpenGL::DrawFrostedGlass(Canvas &canvas, PixelRect rc,
                         const BulkPixelPoint *points,
                         unsigned num_points) noexcept
{
  if (rc.GetWidth() == 0 || rc.GetHeight() == 0 || num_points < 3)
    return;

  auto &r = Prepare(canvas);
  if (!r.valid)
    return;

  /* draw the polygon with the matching part of the blurred screen;
     the linear filter smooths the enlargement */
  GLTexture &result = r.pass[0].GetTexture();
  const PixelSize allocated = result.GetAllocatedSize();

  /* texture coordinates: the used part of the (flipped) texture
     spans the screen; the polygon is in canvas coordinates */
  const float u_max = float(r.small_size.width) / allocated.width;
  const float v_max = float(r.small_size.height) / allocated.height;
  const float u_scale = u_max / r.screen_size.width;
  const float v_scale = v_max / r.screen_size.height;
  const PixelPoint origin = translate;

  std::vector<GLfloat> coord;
  coord.reserve(num_points * 2);
  for (unsigned i = 0; i < num_points; ++i) {
    const int x = points[i].x + origin.x, y = points[i].y + origin.y;
    coord.push_back(x * u_scale);
    coord.push_back(result.IsFlipped()
                    ? v_max - y * v_scale
                    : y * v_scale);
  }

  texture_shader->Use();
  result.Bind();

  const ScopeVertexPointer vp(points);
  glEnableVertexAttribArray(Attribute::TEXCOORD);
  glVertexAttribPointer(Attribute::TEXCOORD, 2, GL_FLOAT, GL_FALSE,
                        0, coord.data());
  glDrawArrays(GL_TRIANGLE_FAN, 0, num_points);
  glDisableVertexAttribArray(Attribute::TEXCOORD);
}

void
OpenGL::DeinitFrostedGlass() noexcept
{
  delete resources;
  resources = nullptr;
}
