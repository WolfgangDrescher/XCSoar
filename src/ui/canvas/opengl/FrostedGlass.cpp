// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FrostedGlass.hpp"
#include "Globals.hpp"
#include "Texture.hpp"
#include "Shaders.hpp"
#include "Program.hpp"
#include "VertexPointer.hpp"
#include "Triangulate.hpp"
#include "ui/canvas/BufferCanvas.hpp"
#include "util/AllocatedArray.hxx"
#include "ui/dim/BulkPoint.hpp"
#include "Math/Point2D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

using OpenGL::FrostedGlassMotion;

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
  unsigned frame_serial = 0;

  /**
   * The frame buffer the result was made from: the screen or a
   * #BufferCanvas.
   */
  GLint framebuffer = 0;

  bool valid = false;

  /**
   * Where the map was when the result was made, see
   * OpenGL::HoldFrostedGlass().
   */
  FrostedGlassMotion motion{{0, 0}, {0, 0}, 1};

  /**
   * The map painted below when the result was made, see
   * OpenGL::FrostedGlassMapPainted().
   */
  unsigned map_generation = 0;
  PixelPoint map_origin{0, 0};

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

/**
 * The screen and the map's #BufferCanvas each keep a set of their
 * own: with a single set, alternating between the two sizes would
 * reallocate all textures twice per frame.
 */
static constexpr std::size_t MAX_RESOURCES = 2;

static std::array<std::unique_ptr<FrostedGlassResources>,
                  MAX_RESOURCES> resources;

/**
 * The map motion while the results are held, see
 * OpenGL::HoldFrostedGlass().
 */
static std::optional<FrostedGlassMotion> hold;

static constexpr FrostedGlassMotion NO_MOTION{{0, 0}, {0, 0}, 1};

/**
 * The frame the map was last painted onto the screen in, and the
 * generation and origin it had then, see
 * OpenGL::FrostedGlassMapPainted().
 */
static unsigned map_frame_serial = ~0u;
static unsigned map_generation = 0;
static PixelPoint map_origin{0, 0};

/**
 * Was the map painted in this frame, unchanged since the result was
 * made?  Then the screen below the surfaces is the same.
 */
static bool
IsMapUnchanged(const FrostedGlassResources &r) noexcept
{
  return map_frame_serial == OpenGL::frame_serial &&
    r.map_generation == map_generation && r.map_origin == map_origin;
}

/**
 * Has the map stayed close enough to where it was when the result
 * was made for the result to be reused?  Further away, its stretched
 * edges would show.
 */
static bool
IsNear(const FrostedGlassMotion &made, const FrostedGlassMotion &now,
       PixelSize screen_size) noexcept
{
  const float ratio = now.zoom / made.zoom;
  if (ratio < 0.8f || ratio > 1.25f)
    return false;

  const auto d = now.offset - made.offset;
  return unsigned(std::abs(d.x)) <= screen_size.width / 8 &&
    unsigned(std::abs(d.y)) <= screen_size.height / 8;
}

/**
 * The set made for the given frame buffer, else an unused or the
 * least recently used one.
 */
static FrostedGlassResources &
FindResources(GLint framebuffer) noexcept
{
  auto *lru = &resources.front();

  for (auto &i : resources) {
    if (!i) {
      i = std::make_unique<FrostedGlassResources>();
      i->framebuffer = framebuffer;
      return *i;
    }

    if (i->framebuffer == framebuffer)
      return *i;

    if (i->frame_serial < (*lru)->frame_serial)
      lru = &i;
  }

  auto &r = **lru;
  r.valid = false;
  r.framebuffer = framebuffer;
  return r;
}

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
  const PixelSize screen_size{OpenGL::viewport_size.x,
                              OpenGL::viewport_size.y};

  GLint framebuffer = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);

  auto &r = FindResources(framebuffer);

  if (r.valid && r.screen_size == screen_size &&
      (r.frame_serial == OpenGL::frame_serial || IsMapUnchanged(r) ||
       (hold && IsNear(r.motion, *hold, screen_size))))
    return r;

  r.valid = false;
  r.frame_serial = OpenGL::frame_serial;
  r.motion = hold ? *hold : NO_MOTION;
  r.map_generation = map_generation;
  r.map_origin = map_origin;
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

  /* 1. grab the whole frame buffer; the canvas may be a SubCanvas,
     so address it relative to the canvas origin */
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
OpenGL::PrepareFrostedGlass(Canvas &canvas) noexcept
{
  Prepare(canvas);
}

void
OpenGL::DrawFrostedGlass(Canvas &canvas, PixelRect rc,
                         const BulkPixelPoint *points,
                         unsigned num_points, bool convex) noexcept
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

  /* while the map is being dragged or zoomed (see HoldFrostedGlass()),
     the copy shows the map where it was when the copy was made; find
     the map content now under each point there */
  const bool moved = hold &&
    (hold->offset != r.motion.offset || hold->zoom != r.motion.zoom);
  const float k = moved ? r.motion.zoom / hold->zoom : 1;

  std::vector<GLfloat> coord;
  coord.reserve(num_points * 2);
  for (unsigned i = 0; i < num_points; ++i) {
    float x = points[i].x + origin.x, y = points[i].y + origin.y;
    if (moved) {
      x = hold->center.x + k * (x - hold->center.x - hold->offset.x)
        + r.motion.offset.x;
      y = hold->center.y + k * (y - hold->center.y - hold->offset.y)
        + r.motion.offset.y;
    }

    coord.push_back(x * u_scale);
    coord.push_back(result.IsFlipped()
                    ? v_max - y * v_scale
                    : y * v_scale);
  }

  /* a concave polygon, e.g. the thermal band, must be triangulated;
     a convex one is a triangle fan as it is */
  static AllocatedArray<GLushort> triangle_buffer;
  unsigned idx_count = 0;
  if (!convex) {
    idx_count = PolygonToTriangles(points, num_points, triangle_buffer);
    if (idx_count == 0)
      return;
  }

  texture_shader->Use();
  result.Bind();

  const ScopeVertexPointer vp(points);
  glEnableVertexAttribArray(Attribute::TEXCOORD);
  glVertexAttribPointer(Attribute::TEXCOORD, 2, GL_FLOAT, GL_FALSE,
                        0, coord.data());
  if (convex)
    glDrawArrays(GL_TRIANGLE_FAN, 0, num_points);
  else
    glDrawElements(GL_TRIANGLES, idx_count, GL_UNSIGNED_SHORT,
                   triangle_buffer.data());
  glDisableVertexAttribArray(Attribute::TEXCOORD);
}

void
OpenGL::FrostedGlassMapPainted(unsigned generation,
                               PixelPoint origin) noexcept
{
  map_frame_serial = frame_serial;
  map_generation = generation;
  map_origin = origin;
}

void
OpenGL::HoldFrostedGlass(const FrostedGlassMotion *motion) noexcept
{
  if (motion != nullptr)
    hold = *motion;
  else
    hold.reset();
}

void
OpenGL::DeinitFrostedGlass() noexcept
{
  for (auto &i : resources)
    i.reset();
}
