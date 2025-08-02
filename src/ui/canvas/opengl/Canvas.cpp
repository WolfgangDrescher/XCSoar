// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Canvas.hpp"
#include "Triangulate.hpp"
#include "Globals.hpp"
#include "Texture.hpp"
#include "Scope.hpp"
#include "VertexArray.hpp"
#include "Buffer.hpp"
#include "VertexPointer.hpp"
#include "ExactPixelPoint.hpp"
#include "ui/canvas/custom/Cache.hpp"
#include "ui/canvas/Bitmap.hpp"
#include "ui/canvas/Util.hpp"
#include "Screen/Layout.hpp"
#include "Math/Angle.hpp"
#include "util/AllocatedArray.hxx"
#include "util/Macros.hpp"
#include "util/UTF8.hpp"
#include "LogFile.hpp"
#include "Shaders.hpp"
#include "Program.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef UNICODE
#include "util/ConvertString.hpp"
#endif

#ifndef NDEBUG
#include "util/UTF8.hpp"
#endif

#include <cassert>

AllocatedArray<BulkPixelPoint> Canvas::vertex_buffer;

static void
GLDrawRectangle(const PixelRect r) noexcept
{
  /* can't use glRecti() with GLSL because it bypasses the vertex
     shader */

  const BulkPixelPoint vertices[] = {
    {r.left, r.top},
    {r.right, r.top},
    {r.left, r.bottom},
    {r.right, r.bottom},
  };

  const ScopeVertexPointer vp{vertices};
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CKCK OpenGL error 0x%X", err0);
}

void
Canvas::InvertRectangle(PixelRect r) noexcept
{
  /** Inverts rectangle using GL blending effects (hardware accelerated):
   *
   * Drawing white (Draw_color=1,1,1) rectangle over the image with GL_ONE_MINUS_DST_COLOR
   * blending function yields New_DST_color= Draw_Color*(1-Old_DST_Color)
   *
   */

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // Make sure alpha channel is not damaged
  GLenum err4 = glGetError(); if (err4 != GL_NO_ERROR) LogFormat("CLCL OpenGL error 0x%X", err4);

  glEnable(GL_BLEND);
  GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("CMCM OpenGL error 0x%X", err3);
  glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); // DST is overwritten part of image = old_DST_color
  GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("CNCN OpenGL error 0x%X", err2);

  const Color cwhite(0xff, 0xff, 0xff); // Draw color white (source channel of blender)

  DrawFilledRectangle(r, cwhite);

  glDisable(GL_BLEND);
  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("COCO OpenGL error 0x%X", err1);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CPCP OpenGL error 0x%X", err0);
}

static tstring_view
ClipText(const Font &font, tstring_view text,
         int x, unsigned canvas_width) noexcept
{
  if (text.empty() || x >= int(canvas_width))
    return {};

  /* this is an approximation, just good enough for clipping */
  unsigned font_width = std::max(font.GetHeight() / 4U, 1U);

  unsigned max_width = canvas_width - x;
  unsigned max_chars = max_width / font_width;

  return text.substr(0, TruncateStringUTF8(text, max_chars));
}

void
Canvas::DrawFilledRectangle(PixelRect r, const Color color) noexcept
{
  assert(offset == OpenGL::translate);

  OpenGL::solid_shader->Use();

  color.Bind();

  GLDrawRectangle(r);
}

void
Canvas::DrawOutlineRectangleGL(PixelRect r) noexcept
{
  --r.right;
  --r.bottom;

  const ExactPixelPoint vertices[] = {
    r.GetTopLeft(),
    r.GetTopRight(),
    r.GetBottomRight(),
    r.GetBottomLeft(),
  };

  const ScopeVertexPointer vp(vertices);
  glDrawArrays(GL_LINE_LOOP, 0, 4);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CQCQ OpenGL error 0x%X", err0);
}

void
Canvas::DrawOutlineRectangle(PixelRect r) noexcept
{
  OpenGL::solid_shader->Use();

  pen.Bind();
  DrawOutlineRectangleGL(r);
  pen.Unbind();
}

void
Canvas::DrawOutlineRectangle(PixelRect r, Color color) noexcept
{
  OpenGL::solid_shader->Use();

  color.Bind();
  glLineWidth(1);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CRCR OpenGL error 0x%X", err0);

  DrawOutlineRectangleGL(r);
}

void
Canvas::FadeToWhite(GLubyte alpha) noexcept
{
  const ScopeAlphaBlend alpha_blend;
  const Color color(0xff, 0xff, 0xff, alpha);
  Clear(color);
}

void
Canvas::FadeToWhite(PixelRect rc, GLubyte alpha) noexcept
{
  const ScopeAlphaBlend alpha_blend;
  const Color color(0xff, 0xff, 0xff, alpha);
  DrawFilledRectangle(rc, color);
}

void
Canvas::DrawPolyline(const BulkPixelPoint *points, unsigned num_points) noexcept
{
  OpenGL::solid_shader->Use();

  pen.Bind();

  const ScopeVertexPointer vp(points);
  glDrawArrays(GL_LINE_STRIP, 0, num_points);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CSCS OpenGL error 0x%X", err0);

  pen.Unbind();
}

void
Canvas::DrawPolygon(const BulkPixelPoint *points, unsigned num_points) noexcept
{
  if (brush.IsHollow() && !pen.IsDefined())
    return;

  OpenGL::solid_shader->Use();

  ScopeVertexPointer vp(points);

  if (!brush.IsHollow() && num_points >= 3) {
    brush.Bind();

    static AllocatedArray<GLushort> triangle_buffer;
    unsigned idx_count = PolygonToTriangles(points, num_points,
                                            triangle_buffer);
    if (idx_count > 0)
      glDrawElements(GL_TRIANGLES, idx_count, GL_UNSIGNED_SHORT,
                     triangle_buffer.data());
					 GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CTCT OpenGL error 0x%X", err0);
  }

  if (IsPenOverBrush()) {
    pen.Bind();

    if (pen.GetWidth() <= 2) {
      glDrawArrays(GL_LINE_LOOP, 0, num_points);
	  GLenum err4 = glGetError(); if (err4 != GL_NO_ERROR) LogFormat("CUCU OpenGL error 0x%X", err4);
    } else {
      unsigned vertices = LineToTriangles(points, num_points, vertex_buffer,
                                          pen.GetWidth(), true);
      if (vertices > 0) {
        vp.Update(vertex_buffer.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices);
		GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("CVCV OpenGL error 0x%X", err5);
      }
    }

    pen.Unbind();
  }
}

void
Canvas::DrawTriangleFan(const BulkPixelPoint *points, unsigned num_points) noexcept
{
  if (brush.IsHollow() && !pen.IsDefined())
    return;

  OpenGL::solid_shader->Use();

  ScopeVertexPointer vp(points);

  if (!brush.IsHollow() && num_points >= 3) {
    brush.Bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, num_points);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CWCW OpenGL error 0x%X", err0);
  }

  if (IsPenOverBrush()) {
    pen.Bind();

    if (pen.GetWidth() <= 2) {
      glDrawArrays(GL_LINE_LOOP, 0, num_points);
	  GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("CXCX OpenGL error 0x%X", err5);
    } else {
      unsigned vertices = LineToTriangles(points, num_points, vertex_buffer,
                                          pen.GetWidth(), true);
      if (vertices > 0) {
        vp.Update(vertex_buffer.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices);
		GLenum err6 = glGetError(); if (err6 != GL_NO_ERROR) LogFormat("CYCY OpenGL error 0x%X", err6);
      }
    }

    pen.Unbind();
  }
}

void
Canvas::DrawHLine(int x1, int x2, int y, Color color) noexcept
{
  color.Bind();

  const BulkPixelPoint v[] = {
    { GLvalue(x1), GLvalue(y) },
    { GLvalue(x2), GLvalue(y) },
  };

  const ScopeVertexPointer vp(v);
  glDrawArrays(GL_LINE_STRIP, 0, ARRAY_SIZE(v));
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("CZCZ OpenGL error 0x%X", err0);
}

[[gnu::pure]]
static glm::vec4
ToNormalisedDeviceCoordinates(PixelPoint p) noexcept
{
  p += OpenGL::translate;
  p -= PixelPoint{OpenGL::viewport_size / 2};

  return glm::vec4{p.x, p.y, 0, 1} * OpenGL::projection_matrix;
}

void
Canvas::DrawLine(PixelPoint a, PixelPoint b) noexcept
{
  OpenGL::solid_shader->Use();

  pen.Bind();

  if (pen.GetStyle() != Pen::SOLID) {
    /* this kludge implements dashed lines using a special shader that
       calculates the distance from the start of the line to the
       current pixel and then determines whether to draw the pixel */

    OpenGL::dashed_shader->Use();

    GLfloat period = 1, ratio = 1;
    switch (pen.GetStyle()) {
    case Pen::SOLID:
      break;

    case Pen::DASH1:
    case Pen::DASH2:
    case Pen::DASH3:
      period = 32;
      ratio = 0.6;
      break;
    }

    glUniform1f(OpenGL::dashed_period, period);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DADA OpenGL error 0x%X", err0);
    glUniform1f(OpenGL::dashed_ratio, ratio);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("DBDB OpenGL error 0x%X", err1);

    const glm::vec4 start = ToNormalisedDeviceCoordinates(a);
    glUniform2f(OpenGL::dashed_start, start.x, start.y);
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("DCDC OpenGL error 0x%X", err2);
  }

  const BulkPixelPoint v[] = { a, b };
  const ScopeVertexPointer vp(v);
  glDrawArrays(GL_LINE_STRIP, 0, ARRAY_SIZE(v));
  GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("DEDE OpenGL error 0x%X", err3);

  pen.Unbind();
}

void
Canvas::DrawExactLine(PixelPoint a, PixelPoint b) noexcept
{
  OpenGL::solid_shader->Use();

  pen.Bind();

  const ExactPixelPoint v[] = { a, b };
  const ScopeVertexPointer vp(v);
  glDrawArrays(GL_LINE_STRIP, 0, ARRAY_SIZE(v));
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DFDF OpenGL error 0x%X", err0);

  pen.Unbind();
}

/**
 * Draw a line from a to b, using triangle caps if pen-size > 2 to hide
 * gaps between consecutive lines.
 */
void
Canvas::DrawLinePiece(const PixelPoint a, const PixelPoint b) noexcept
{
  OpenGL::solid_shader->Use();

  pen.Bind();

  const BulkPixelPoint v[] = { {a.x, a.y}, {b.x, b.y} };
  if (pen.GetWidth() > 2) {
    unsigned strip_len = LineToTriangles(v, 2, vertex_buffer, pen.GetWidth(),
                                         false, true);
    if (strip_len > 0) {
      const ScopeVertexPointer vp{vertex_buffer.data()};
      glDrawArrays(GL_TRIANGLE_STRIP, 0, strip_len);
	  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DGDG OpenGL error 0x%X", err0);
    }
  } else {
    const ScopeVertexPointer vp(v);
    glDrawArrays(GL_LINE_STRIP, 0, 2);
	GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("DHDH OpenGL error 0x%X", err3);
  }

  pen.Unbind();
}

void
Canvas::DrawTwoLines(PixelPoint a, PixelPoint b, PixelPoint c) noexcept
{
  OpenGL::solid_shader->Use();

  pen.Bind();

  const BulkPixelPoint v[] = { a, b, c };
  const ScopeVertexPointer vp(v);
  glDrawArrays(GL_LINE_STRIP, 0, ARRAY_SIZE(v));
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DIDI OpenGL error 0x%X", err0);

  pen.Unbind();
}

void
Canvas::DrawTwoLinesExact(PixelPoint a, PixelPoint b, PixelPoint c) noexcept
{
  OpenGL::solid_shader->Use();

  pen.Bind();

  const ExactPixelPoint v[] = { a, b, c };
  const ScopeVertexPointer vp(v);
  glDrawArrays(GL_LINE_STRIP, 0, ARRAY_SIZE(v));
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DJDJ OpenGL error 0x%X", err0);

  pen.Unbind();
}

void
Canvas::DrawCircle(PixelPoint center, unsigned radius) noexcept
{
  if (brush.IsHollow()) {
    OpenGL::circle_outline_shader->Use();
    pen.GetColor().Uniform(OpenGL::circle_outline_color);

    glUniform2f(OpenGL::circle_outline_center, center.x, center.y);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("DKDK OpenGL error 0x%X", err1);
    glUniform1f(OpenGL::circle_outline_radius2, radius);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DKDK OpenGL error 0x%X", err0);
    glUniform1f(OpenGL::circle_outline_radius1, radius - pen.GetWidth());
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("DKDK OpenGL error 0x%X", err2);

    GLDrawRectangle(PixelRect{center}.WithMargin(radius));
  } else {
    OpenGL::filled_circle_shader->Use();
    pen.GetColor().Uniform(OpenGL::filled_circle_color2);
    brush.BindUniform(OpenGL::filled_circle_color1);

    glUniform2f(OpenGL::filled_circle_center, center.x, center.y);
	GLenum err4 = glGetError(); if (err4 != GL_NO_ERROR) LogFormat("DLDL OpenGL error 0x%X", err4);
    glUniform1f(OpenGL::filled_circle_radius2, radius);
	GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("DLDL OpenGL error 0x%X", err5);
    glUniform1f(OpenGL::filled_circle_radius1, radius - pen.GetWidth());
	GLenum err6 = glGetError(); if (err6 != GL_NO_ERROR) LogFormat("DLDL OpenGL error 0x%X", err6);

    GLDrawRectangle(PixelRect{center}.WithMargin(radius));
  }
}

void
Canvas::DrawSegment(PixelPoint center, unsigned radius,
                    Angle start, Angle end, bool horizon) noexcept
{
  ::Segment(*this, center, radius, start, end, horizon);
}

void
Canvas::DrawArc(PixelPoint center, unsigned radius,
                Angle start, Angle end) noexcept
{
  ::Arc(*this, center, radius, start, end);
}

[[gnu::const]]
static unsigned
AngleToDonutVertex(Angle angle) noexcept
{
  return GLDonutVertices::ImportAngle(NATIVE_TO_INT(angle.Native())
                                      + ISINETABLE.size() * 3u / 4u,
                                      ISINETABLE.size());
}

[[gnu::const]]
static std::pair<unsigned,unsigned>
AngleToDonutVertices(Angle start, Angle end) noexcept
{
  static constexpr Angle epsilon = Angle::FullCircle()
    / int(GLDonutVertices::CIRCLE_SIZE * 4u);

  const Angle delta = end - start;

  if (fabs(delta.AsDelta().Native()) <= epsilon.Native())
    /* full circle */
    return std::make_pair(0u, unsigned(GLDonutVertices::MAX_ANGLE));

  const unsigned istart = AngleToDonutVertex(start);
  unsigned iend = AngleToDonutVertex(end);

  if (istart == iend && delta > epsilon) {
    if (end - start >= Angle::HalfCircle())
      /* nearly full circle, round down the end */
      iend = GLDonutVertices::PreviousAngle(iend);
    else
      /* slightly larger than epsilon: draw at least two indices */
      iend = GLDonutVertices::NextAngle(iend);
  }

  return std::make_pair(istart, iend);
}

void
Canvas::DrawAnnulus(PixelPoint center,
                    unsigned small_radius, unsigned big_radius,
                    Angle start, Angle end) noexcept
{
  if (1 == 1) {
    /* TODO: switched to the unoptimised generic implementation due to
       TRAC #2221, caused by rounding error of start/end radial;
       should reimplement GLDonutVertices to use the exact start/end
       radial */
    ::Annulus(*this, center, big_radius, start, end, small_radius);
    return;
  }

  ScopeVertexPointer vp;
  GLDonutVertices vertices(center.x, center.y, small_radius, big_radius);

  const std::pair<unsigned,unsigned> i = AngleToDonutVertices(start, end);
  const unsigned istart = i.first;
  const unsigned iend = i.second;

  if (!brush.IsHollow()) {
    brush.Bind();
    vertices.Bind(vp);

    if (istart > iend) {
      glDrawArrays(GL_TRIANGLE_STRIP, istart,
                   GLDonutVertices::MAX_ANGLE - istart + 2);
		GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err0);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, iend + 2);
	  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err1);
    } else {
		glDrawArrays(GL_TRIANGLE_STRIP, istart, iend - istart + 2);
		GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err2);
    }
  }

  GLenum err6 = glGetError(); if (err6 != GL_NO_ERROR) LogFormat("DMDM OpenGL error 0x%X", err6);

  if (IsPenOverBrush()) {
    pen.Bind();

    if (istart != iend && iend != GLDonutVertices::MAX_ANGLE) {
      if (brush.IsHollow())
        vertices.Bind(vp);

      glDrawArrays(GL_LINE_STRIP, istart, 2);
	  GLenum err7 = glGetError(); if (err7 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err7);
      glDrawArrays(GL_LINE_STRIP, iend, 2);
	  GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("DNDN OpenGL error 0x%X", err5);
    }

    const unsigned pstart = istart / 2;
    const unsigned pend = iend / 2;

    vertices.BindInnerCircle(vp);
    if (pstart < pend) {
      glDrawArrays(GL_LINE_STRIP, pstart, pend - pstart + 1);
	  GLenum err8 = glGetError(); if (err8 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err8);
    } else {
		glDrawArrays(GL_LINE_STRIP, pstart,
			GLDonutVertices::CIRCLE_SIZE - pstart + 1);
		GLenum err9 = glGetError(); if (err9 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err9);
		glDrawArrays(GL_LINE_STRIP, 0, pend + 1);
		GLenum err10 = glGetError(); if (err10 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err10);
    }
GLenum err4 = glGetError(); if (err4 != GL_NO_ERROR) LogFormat("DODO OpenGL error 0x%X", err4);
    vertices.BindOuterCircle(vp);
    if (pstart < pend) {
      glDrawArrays(GL_LINE_STRIP, pstart, pend - pstart + 1);
	  GLenum err11 = glGetError(); if (err11 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err11);
    } else {
		glDrawArrays(GL_LINE_STRIP, pstart,
			GLDonutVertices::CIRCLE_SIZE - pstart + 1);
		GLenum err12 = glGetError(); if (err12 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err12);
		glDrawArrays(GL_LINE_STRIP, 0, pend + 1);
		GLenum err13 = glGetError(); if (err13 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err13);
    }
GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("DPDP OpenGL error 0x%X", err3);
    pen.Unbind();
  }
}

void
Canvas::DrawKeyhole(PixelPoint center,
                    unsigned small_radius, unsigned big_radius,
                    Angle start, Angle end) noexcept
{
  ::KeyHole(*this, center, big_radius, start, end, small_radius);
}

void
Canvas::DrawFocusRectangle(PixelRect rc) noexcept
{
  DrawOutlineRectangle(rc, COLOR_DARK_GRAY);
}

const PixelSize
Canvas::CalcTextSize(tstring_view text) const noexcept
{
#ifdef UNICODE
  const WideToUTF8Converter text2(text);
#else
  const std::string_view text2 = text;
  assert(ValidateUTF8(text));
#endif

  PixelSize size = { 0, 0 };

  if (font == nullptr)
    return size;

  /* see if the TextCache can handle this request */
  size = TextCache::LookupSize(*font, text2);
  if (size.height > 0)
    return size;

  return TextCache::GetSize(*font, text2);
}

/**
 * Prepare drawing a GL_ALPHA texture with the specified color.
 */
static void
PrepareColoredAlphaTexture(Color color) noexcept
{
  OpenGL::alpha_shader->Use();
  color.Bind();
}

void
Canvas::DrawText(PixelPoint p, tstring_view text) noexcept
{
#ifdef UNICODE
  const WideToUTF8Converter text2(text);
#else
  const std::string_view text2 = text;
  assert(ValidateUTF8(text));
#endif

  assert(offset == OpenGL::translate);

  if (font == nullptr)
    return;

  const std::string_view text3 = ClipText(*font, text2, p.x, size.width);
  if (text3.empty())
    return;

  GLTexture *texture = TextCache::Get(*font, text3);
  if (texture == nullptr)
    return;

  if (background_mode == OPAQUE)
    DrawFilledRectangle({p, texture->GetSize()}, background_color);

  PrepareColoredAlphaTexture(text_color);

  const ScopeAlphaBlend alpha_blend;

  texture->Bind();
  texture->Draw(p);
}

void
Canvas::DrawTransparentText(PixelPoint p, tstring_view text) noexcept
{
#ifdef UNICODE
  const WideToUTF8Converter text2(text);
#else
  const std::string_view text2 = text;
  assert(ValidateUTF8(text));
#endif

  assert(offset == OpenGL::translate);

  if (font == nullptr)
    return;

  const std::string_view text3 = ClipText(*font, text2, p.x, size.width);
  if (text3.empty())
    return;

  GLTexture *texture = TextCache::Get(*font, text3);
  if (texture == nullptr)
    return;

  PrepareColoredAlphaTexture(text_color);

  const ScopeAlphaBlend alpha_blend;

  texture->Bind();
  texture->Draw(p);
}

void
Canvas::DrawClippedText(PixelPoint p, PixelSize size,
                        tstring_view text) noexcept
{
#ifdef UNICODE
  const WideToUTF8Converter text2(text);
#else
  const std::string_view text2 = text;
  assert(ValidateUTF8(text));
#endif

  assert(offset == OpenGL::translate);

  if (font == nullptr)
    return;

  const std::string_view text3 = ClipText(*font, text2, 0, size.width);
  if (text3.empty())
    return;

  GLTexture *texture = TextCache::Get(*font, text3);
  if (texture == nullptr)
    return;

  if (texture->GetHeight() < size.height)
    size.height = texture->GetHeight();
  if (texture->GetWidth() < size.width)
    size.width = texture->GetWidth();

  PrepareColoredAlphaTexture(text_color);

  const ScopeAlphaBlend alpha_blend;

  texture->Bind();
  texture->Draw({p, size}, PixelRect{size});
}

void
Canvas::Stretch(PixelPoint dest_position, PixelSize dest_size,
                const GLTexture &texture,
                PixelPoint src_position, PixelSize src_size) noexcept
{
  assert(offset == OpenGL::translate);

  OpenGL::texture_shader->Use();

  texture.Draw({dest_position, dest_size}, {src_position, src_size});
}

void
Canvas::Stretch(PixelPoint dest_position, PixelSize dest_size,
                const GLTexture &texture) noexcept
{
  Stretch(dest_position, dest_size,
          texture, {0, 0}, texture.GetSize());
}

void
Canvas::Copy(PixelPoint dest_position, PixelSize dest_size,
             const Bitmap &src, PixelPoint src_position) noexcept
{
  Stretch(dest_position, dest_size,
          src, src_position, dest_size);
}

void
Canvas::Copy(const Bitmap &src) noexcept
{
  Copy({0, 0}, src.GetSize(), src, {0, 0});
}

void
Canvas::StretchNot(const Bitmap &src) noexcept
{
  assert(src.IsDefined());

  OpenGL::invert_shader->Use();

  GLTexture &texture = *src.GetNative();
  texture.Bind();
  texture.Draw(GetRect(), PixelRect(src.GetSize()));
}

void
Canvas::Stretch(PixelPoint dest_position, PixelSize dest_size,
                const Bitmap &src,
                PixelPoint src_position, PixelSize src_size) noexcept
{
  assert(offset == OpenGL::translate);
  assert(src.IsDefined());

  OpenGL::texture_shader->Use();

  GLTexture &texture = *src.GetNative();
  texture.Bind();
  texture.Draw({dest_position, dest_size}, {src_position, src_size});
}

void
Canvas::Stretch(PixelPoint dest_position, PixelSize dest_size,
                const Bitmap &src) noexcept
{
  assert(offset == OpenGL::translate);
  assert(src.IsDefined());

  OpenGL::texture_shader->Use();

  GLTexture &texture = *src.GetNative();
  texture.Bind();

  texture.Draw({dest_position, dest_size}, PixelRect{src.GetSize()});
}

void
Canvas::StretchMono(PixelPoint dest_position, PixelSize dest_size,
                    const Bitmap &src,
                    PixelPoint src_position, PixelSize src_size,
                    Color fg_color, [[maybe_unused]] Color bg_color) noexcept
{
  /* note that this implementation ignores the background color; it is
     not mandatory, and we can assume that the background is already
     set; it is only being passed to this function because the GDI
     implementation will be faster when erasing the background
     again */

  PrepareColoredAlphaTexture(fg_color);

  const ScopeAlphaBlend alpha_blend;

  GLTexture &texture = *src.GetNative();
  texture.Bind();
  texture.Draw({dest_position, dest_size}, {src_position, src_size});
}

void
Canvas::CopyToTexture(GLTexture &texture, PixelRect src_rc) const noexcept
{
  assert(offset == OpenGL::translate);

  texture.Bind();
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                      OpenGL::translate.x + src_rc.left,
                      OpenGL::viewport_size.y - OpenGL::translate.y - src_rc.bottom,
                      src_rc.GetWidth(), src_rc.GetHeight());
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DQDQ OpenGL error 0x%X", err0);

}

void
Canvas::DrawRoundRectangle(PixelRect r, PixelSize ellipse_size) noexcept
{
  unsigned radius = std::min(std::min(ellipse_size.width, ellipse_size.height),
                             std::min(r.GetWidth(),
                                      r.GetHeight())) / 2u;
  ::RoundRect(*this, r, radius);
}
