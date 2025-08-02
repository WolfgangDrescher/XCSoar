// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Texture.hpp"
#include "Globals.hpp"
#include "VertexPointer.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Scope.hpp"
#include "LogFile.hpp"

#include <glm/gtc/type_ptr.hpp>

#ifdef ENABLE_SDL
#include <SDL.h>
#endif

#include <cassert>

static constexpr unsigned
NextPowerOfTwo(unsigned i) noexcept
{
  unsigned p = 1;
  while (p < i)
    p <<= 1;
  return p;
}

[[gnu::const]]
static inline unsigned
ValidateTextureSize(unsigned i) noexcept
{
  return OpenGL::texture_non_power_of_two ? i : NextPowerOfTwo(i);
}

[[gnu::const]]
static inline PixelSize
ValidateTextureSize(PixelSize size) noexcept
{
  return { ValidateTextureSize(size.width), ValidateTextureSize(size.height) };
}

/**
 * Load data into the current texture.  Fixes alignment to the next
 * power of two if needed.
 */
static void
LoadTextureAutoAlign(GLint internal_format, PixelSize size,
                     GLenum format, GLenum type, const GLvoid *pixels) noexcept
{
  assert(pixels != nullptr);

  PixelSize validated_size = ValidateTextureSize(size);

  if (validated_size == size)
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, size.width, size.height, 0,
                 format, type, pixels);
GLenum err = glGetError();
if (err != GL_NO_ERROR) {
  LogFormat("AAA OpenGL error 0x%X", err);
}
  else {
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                 validated_size.width, validated_size.height, 0,
                 format, type, nullptr);
				 GLenum err = glGetError();
if (err != GL_NO_ERROR) {
  LogFormat("BBB OpenGL error 0x%X", err);
}
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width, size.height,
                    format, type, pixels);
					GLenum err2 = glGetError();
if (err2 != GL_NO_ERROR) {
  LogFormat("CCC OpenGL error 0x%X", err2);
}
  }
}

GLTexture::GLTexture(GLint internal_format, PixelSize _size,
                     GLenum format, GLenum type,
                     bool _flipped) noexcept
  :size(_size), allocated_size(ValidateTextureSize(_size)), flipped(_flipped)
{
  Initialise();

  glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
               allocated_size.width, allocated_size.height,
               0, format, type, nullptr);
			   					GLenum err = glGetError();
if (err != GL_NO_ERROR) {
  LogFormat("DDD OpenGL error 0x%X", err);
}
}

GLTexture::GLTexture(GLint internal_format, PixelSize _size,
                     GLenum format, GLenum type, const GLvoid *data,
                     bool _flipped) noexcept
  :size(_size), allocated_size(ValidateTextureSize(_size)), flipped(_flipped)
{
  Initialise();
  LoadTextureAutoAlign(internal_format, size, format, type, data);
}

void
GLTexture::ResizeDiscard([[maybe_unused]] GLint internal_format, PixelSize new_size,
                         GLenum format, GLenum type) noexcept
{
  const PixelSize validated_size = ValidateTextureSize(new_size);
  const PixelSize old_size = GetAllocatedSize();

  size = new_size;

  if (validated_size == old_size)
    return;

  allocated_size = validated_size;

  Bind();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
               validated_size.width, validated_size.height,
               0, format, type, nullptr);
			   					GLenum err = glGetError();
if (err != GL_NO_ERROR) {
  LogFormat("EEE OpenGL error 0x%X", err);
}
}

void
GLTexture::Initialise() noexcept
{
  glGenTextures(1, &id);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FWAFWA OpenGL error 0x%X", err0);
  Bind();
  Configure();
}

void
GLTexture::Configure() noexcept
{
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("FWCFWC OpenGL error 0x%X", err3);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("FWBFWB OpenGL error 0x%X", err2);

  constexpr GLint filter = GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("FWAFWA OpenGL error 0x%X", err1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FWFW OpenGL error 0x%X", err0);
}

void
GLTexture::Draw(PixelRect dest, PixelRect src) const noexcept
{
  const BulkPixelPoint vertices[] = {
    dest.GetTopLeft(),
    dest.GetTopRight(),
    dest.GetBottomLeft(),
    dest.GetBottomRight(),
  };

  const ScopeVertexPointer vp(vertices);

  const PixelSize allocated = GetAllocatedSize();
  GLfloat x0 = (GLfloat)src.left / allocated.width;
  GLfloat y0 = (GLfloat)src.top / allocated.height;
  GLfloat x1 = (GLfloat)src.right / allocated.width;
  GLfloat y1 = (GLfloat)src.bottom / allocated.height;

  const GLfloat coord[] = {
    x0, flipped ? y1 : y0,
    x1, flipped ? y1 : y0,
    x0, flipped ? y0 : y1,
    x1, flipped ? y0 : y1,
  };

  glEnableVertexAttribArray(OpenGL::Attribute::TEXCOORD);
  GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("FXFXV OpenGL error 0x%X", err3);
  glVertexAttribPointer(OpenGL::Attribute::TEXCOORD, 2, GL_FLOAT, GL_FALSE,
                        0, coord);
GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("FXFXB OpenGL error 0x%X", err2);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("FXFXA OpenGL error 0x%X", err1);

  glDisableVertexAttribArray(OpenGL::Attribute::TEXCOORD);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FXFX OpenGL error 0x%X", err0);
}
