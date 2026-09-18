// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "BoxShadowRenderer.hpp"
#include "ui/dim/Rect.hpp"

#ifdef ENABLE_OPENGL

#include "Math/Point2D.hpp"
#include "Screen/Layout.hpp"
#include "ui/canvas/Color.hpp"
#include "ui/canvas/opengl/Program.hpp"
#include "ui/canvas/opengl/Scissor.hpp"
#include "ui/canvas/opengl/Scope.hpp"
#include "ui/canvas/opengl/Shaders.hpp"
#include "ui/canvas/opengl/VertexPointer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

namespace {

/**
 * How far the opaque core of the shadow reaches beyond the box, in
 * virtual points.
 */
constexpr int SHADOW_SPREAD = 6;

/**
 * The width of the blurred transition, in virtual points.  It is
 * centered on the edge of the shadow's shape, i.e. the shadow fades
 * out over the last SHADOW_BLUR/2 points and reaches
 * SHADOW_SPREAD+SHADOW_BLUR/2 beyond the box.
 */
constexpr int SHADOW_BLUR = 32;

/**
 * The opacity of the black shadow where it is darkest.
 */
constexpr uint8_t SHADOW_ALPHA = 115;

/**
 * The number of segments each corner arc of a contour is approximated
 * with.
 */
constexpr unsigned CORNER_SEGMENTS = 8;

/**
 * The number of vertices of one contour: four corner arcs, each
 * starting and ending on one of the straight edges.
 */
constexpr unsigned CONTOUR_VERTICES = 4 * (CORNER_SEGMENTS + 1);

/**
 * The maximum number of contours; this limits both the amount of work
 * per shadow and the size of the index buffer.
 */
constexpr unsigned MAX_CONTOURS = 33;

static_assert(MAX_CONTOURS * CONTOUR_VERTICES <= 0x10000,
              "Too many vertices for 16 bit indices");

/**
 * The largest mesh #MAX_CONTOURS can produce: one triangle fan filling
 * the innermost contour, plus two triangles per vertex between each
 * pair of adjacent contours.
 */
constexpr unsigned MAX_VERTICES = MAX_CONTOURS * CONTOUR_VERTICES;
constexpr unsigned MAX_INDICES = (CONTOUR_VERTICES - 2) * 3
  + (MAX_CONTOURS - 1) * CONTOUR_VERTICES * 6;

/**
 * The opacity of a Gaussian blur across its transition, approximated
 * with a smoothstep(): 1 at the inner end (x=0), 0.5 exactly on the
 * edge of the shadow shape (x=0.5) and 0 at the outer end (x=1).
 */
constexpr float
BlurOpacity(float x) noexcept
{
  x = std::clamp(x, 0.f, 1.f);
  return 1.f - x * x * (3.f - 2.f * x);
}

/**
 * The unit circle offsets of one quarter arc, from (1,0) to (0,1).
 */
const auto quarter_arc = []{
  std::array<FloatPoint2D, CORNER_SEGMENTS + 1> result{};
  for (unsigned i = 0; i <= CORNER_SEGMENTS; ++i) {
    const float angle = float(std::numbers::pi / 2) * i / CORNER_SEGMENTS;
    result[i] = {std::cos(angle), std::sin(angle)};
  }
  return result;
}();

/**
 * Emit the contour of all points which have the given distance to the
 * four corner points of #centers - i.e. a rounded rectangle.
 *
 * All contours of one shadow share the same #centers, which makes the
 * triangles between two of them annular sectors: their corner values
 * lie on a plane, so both halves of each quad interpolate the same
 * gradient.  Contours built as nested sharp-cornered rectangles would
 * instead meet in a mitre at each corner, and because that mitre is
 * wider than the straight bands, it shows up as a bright diagonal
 * streak extending each corner.
 *
 * The contour is emitted clockwise, starting on the left edge, and
 * always consists of #CONTOUR_VERTICES vertices, so that two contours
 * can be stitched together with a ring of triangles.
 */
void
AppendContour(FloatPoint2D *dest, const PixelRect &centers,
              float radius) noexcept
{
  const float left = centers.left, top = centers.top;
  const float right = centers.right, bottom = centers.bottom;

  for (const auto i : quarter_arc)
    /* top left, from the left edge to the top edge */
    *dest++ = {left - radius * i.x, top - radius * i.y};

  for (const auto i : quarter_arc)
    /* top right, from the top edge to the right edge */
    *dest++ = {right + radius * i.y, top - radius * i.x};

  for (const auto i : quarter_arc)
    /* bottom right, from the right edge to the bottom edge */
    *dest++ = {right + radius * i.x, bottom + radius * i.y};

  for (const auto i : quarter_arc)
    /* bottom left, from the bottom edge to the left edge */
    *dest++ = {left - radius * i.y, bottom + radius * i.x};
}

/**
 * The blurred shadow of a rounded rectangle, as a triangle mesh whose
 * vertex colours carry the blur gradient: a stack of contours around
 * the same four corner centres, from #inner_radius to #outer_radius.
 *
 * Both buffers are sized for the largest mesh we can produce, so
 * that building one never needs to allocate.
 */
class ShadowMesh {
  std::array<FloatPoint2D, MAX_VERTICES> vertices;
  std::array<Color, MAX_VERTICES> colors;
  std::array<GLushort, MAX_INDICES> indices;
  unsigned n_indices = 0;

public:
  /**
   * @param centers the four points the corner arcs of all contours
   * are centered on
   * @param inner_radius the radius of the innermost contour; with 0,
   * the mesh fills the shape, otherwise it is a ring
   * @param outer_radius the radius of the outermost contour
   * @param edge_radius the radius at which the edge of the blurred
   * shape lies, i.e. where the opacity is half of @p alpha
   * @param blur the width of the blurred transition, in pixels
   * @param alpha the opacity of the black shadow where it is darkest
   */
  ShadowMesh(const PixelRect &centers,
             float inner_radius, float outer_radius, float edge_radius,
             int blur, uint8_t alpha) noexcept;

  void Draw() const noexcept;
};

ShadowMesh::ShadowMesh(const PixelRect &centers,
                       const float inner_radius, const float outer_radius,
                       const float edge_radius,
                       const int blur, const uint8_t alpha) noexcept
{
  /* one contour every two pixels is plenty for a smooth gradient */
  const unsigned n_intervals =
    std::clamp<unsigned>(unsigned((outer_radius - inner_radius) / 2),
                         1, MAX_CONTOURS - 1);
  const unsigned n_contours = n_intervals + 1;

  for (unsigned i = 0; i < n_contours; ++i) {
    const float radius = inner_radius
      + (outer_radius - inner_radius) * i / n_intervals;
    AppendContour(&vertices[i * CONTOUR_VERTICES], centers, radius);

    /* the opacity depends on the distance to the shape's edge */
    const float opacity =
      BlurOpacity((radius - edge_radius + blur / 2.f) / blur);
    const Color color =
      COLOR_BLACK.WithAlpha(uint8_t(std::lround(alpha * opacity)));
    std::fill_n(&colors[i * CONTOUR_VERTICES], CONTOUR_VERTICES, color);
  }

  if (inner_radius <= 0)
    /* fill the innermost contour with a triangle fan */
    for (unsigned i = 1; i + 1 < CONTOUR_VERTICES; ++i) {
      indices[n_indices++] = 0;
      indices[n_indices++] = i;
      indices[n_indices++] = i + 1;
    }

  /* stitch adjacent contours together with a ring of triangles; the
     vertex colors make OpenGL interpolate the blur gradient */
  for (unsigned i = 0; i < n_intervals; ++i) {
    const unsigned a = i * CONTOUR_VERTICES;
    const unsigned b = a + CONTOUR_VERTICES;

    for (unsigned j = 0; j < CONTOUR_VERTICES; ++j) {
      const unsigned j2 = (j + 1) % CONTOUR_VERTICES;

      indices[n_indices++] = a + j;
      indices[n_indices++] = b + j;
      indices[n_indices++] = a + j2;

      indices[n_indices++] = b + j;
      indices[n_indices++] = b + j2;
      indices[n_indices++] = a + j2;
    }
  }

  assert(n_indices <= indices.size());
}

void
ShadowMesh::Draw() const noexcept
{
  const ScopeAlphaBlend alpha_blend;
  const ScopeVertexPointer vp(vertices.data());
  const ScopeColorPointer cp(colors.data());
  OpenGL::solid_shader->Use();
  glDrawElements(GL_TRIANGLES, GLsizei(n_indices),
                 GL_UNSIGNED_SHORT, indices.data());
}

} // anonymous namespace

#endif /* ENABLE_OPENGL */

void
DrawBoxShadow([[maybe_unused]] const PixelRect &rc) noexcept
{
#ifdef ENABLE_OPENGL
  /* the shape which gets blurred: the box, inflated by the spread */
  PixelRect shape = rc;
  shape.Grow(Layout::VptScale(SHADOW_SPREAD));

  const int blur = Layout::VptScale(SHADOW_BLUR);

  /* the blur transition reaches this far outside and inside of the
     shape's edge */
  const int outer = blur / 2;
  const int inner = std::min<int>(outer,
                                  std::min(shape.GetWidth(),
                                           shape.GetHeight()) / 2);

  /* the innermost contour collapses onto the corner centres, and the
     outermost is #inner+#outer away */
  const PixelRect centers{
    shape.left + inner, shape.top + inner,
    shape.right - inner, shape.bottom - inner,
  };

  const ShadowMesh mesh{centers, 0.f, float(inner + outer), float(inner),
                        blur, SHADOW_ALPHA};
  mesh.Draw();
#endif
}

void
DrawBoxHalo([[maybe_unused]] const PixelRect &clip,
            [[maybe_unused]] const PixelRect &box,
            [[maybe_unused]] unsigned corner_radius,
            [[maybe_unused]] unsigned blur,
            [[maybe_unused]] uint8_t alpha) noexcept
{
#ifdef ENABLE_OPENGL
  /* the ring starts on the box's edge: its corner arcs are centered
     on the box's corner centres, with the box's radius */
  const int r = std::min<int>(corner_radius,
                              std::min(box.GetWidth(),
                                       box.GetHeight()) / 2);
  const PixelRect centers{
    box.left + r, box.top + r,
    box.right - r, box.bottom - r,
  };

  const ShadowMesh mesh{centers, float(r), float(r + blur / 2), float(r),
                        int(blur), alpha};

  const GLCanvasScissor scissor{clip};
  mesh.Draw();
#endif
}
