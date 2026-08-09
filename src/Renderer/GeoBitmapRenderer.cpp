// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GeoBitmapRenderer.hpp"
#include "ui/canvas/RawBitmap.hpp"
#include "Geo/GeoBounds.hpp"
#include "Projection/Projection.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Texture.hpp"
#include "ui/canvas/opengl/Scope.hpp"
#include "ui/canvas/opengl/VertexPointer.hpp"
#include "ui/canvas/opengl/Attribute.hpp"
#include "ui/dim/BulkPoint.hpp"

/**
 * Draw a geo-referenced bitmap texture.  The caller is responsible
 * for setting up the OpenGL shader (e.g. via
 * ScopeTextureConstantAlpha) before calling this function.
 */
void
DrawGeoBitmap(const RawBitmap &bitmap, PixelSize bitmap_size,
              const GeoBounds &bounds,
              const Projection &projection)
{
  assert(bounds.IsValid());

  const GLTexture &texture = bitmap.BindAndGetTexture();
  const PixelSize allocated = texture.GetAllocatedSize();

  const GLfloat src_x = 0, src_y = 0, src_width = bitmap_size.width,
    src_height = bitmap_size.height;

  GLfloat x0 = src_x / allocated.width;
  GLfloat y0 = src_y / allocated.height;
  GLfloat x1 = (src_x + src_width) / allocated.width;
  GLfloat y1 = (src_y + src_height) / allocated.height;

  glEnableVertexAttribArray(OpenGL::Attribute::TEXCOORD);

  if (projection.HasScreenTilt()) {
    /* with the perspective tilt, GeoToScreen() is not affine, but the
       GPU interpolates each triangle affinely; subdivide the quad
       into a grid so the interpolation error becomes invisible */
    constexpr unsigned N = 8;

    const Angle north = bounds.GetNorth(), west = bounds.GetWest();
    const Angle height = bounds.GetHeight(), width = bounds.GetWidth();

    const auto ProjectRow = [&](unsigned i, BulkPixelPoint *row) {
      const Angle latitude = north - height * (double(i) / N);
      for (unsigned j = 0; j <= N; ++j) {
        const Angle longitude = west + width * (double(j) / N);
        row[j] = projection.GeoToScreen(GeoPoint(longitude, latitude));
      }
    };

    BulkPixelPoint rows[2][N + 1];
    ProjectRow(0, rows[0]);

    for (unsigned i = 0; i < N; ++i) {
      ProjectRow(i + 1, rows[(i + 1) % 2]);

      BulkPixelPoint vertices[2 * (N + 1)];
      GLfloat coord[4 * (N + 1)];
      const GLfloat ty0 = y0 + (y1 - y0) * (GLfloat(i) / N);
      const GLfloat ty1 = y0 + (y1 - y0) * (GLfloat(i + 1) / N);

      for (unsigned j = 0; j <= N; ++j) {
        const GLfloat tx = x0 + (x1 - x0) * (GLfloat(j) / N);
        vertices[2 * j] = rows[i % 2][j];
        vertices[2 * j + 1] = rows[(i + 1) % 2][j];
        coord[4 * j] = tx;
        coord[4 * j + 1] = ty0;
        coord[4 * j + 2] = tx;
        coord[4 * j + 3] = ty1;
      }

      const ScopeVertexPointer vp(vertices);
      glVertexAttribPointer(OpenGL::Attribute::TEXCOORD, 2, GL_FLOAT,
                            GL_FALSE, 0, coord);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 * (N + 1));
    }
  } else {
    const BulkPixelPoint vertices[] = {
      projection.GeoToScreen(bounds.GetNorthWest()),
      projection.GeoToScreen(bounds.GetNorthEast()),
      projection.GeoToScreen(bounds.GetSouthWest()),
      projection.GeoToScreen(bounds.GetSouthEast()),
    };

    const ScopeVertexPointer vp(vertices);

    const GLfloat coord[] = {
      x0, y0,
      x1, y0,
      x0, y1,
      x1, y1,
    };

    glVertexAttribPointer(OpenGL::Attribute::TEXCOORD, 2, GL_FLOAT, GL_FALSE,
                          0, coord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  glDisableVertexAttribArray(OpenGL::Attribute::TEXCOORD);
}

#endif
