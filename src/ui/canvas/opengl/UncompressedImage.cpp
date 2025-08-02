// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "UncompressedImage.hpp"
#include "Texture.hpp"
#include "ui/canvas/custom/UncompressedImage.hpp"
#include "util/Compiler.h"
#include "LogFile.hpp"
#include <SDL.h>
#include "ui/opengl/System.hpp"
GLTexture *
ImportTexture(const UncompressedImage &image)
{
  GLint internal_format;
  GLenum format, type;

  switch (image.GetFormat()) {
  case UncompressedImage::Format::GRAY:
    internal_format = GL_LUMINANCE;
    format = GL_LUMINANCE;
    type = GL_UNSIGNED_BYTE;
    break;

  case UncompressedImage::Format::RGB:
    internal_format = GL_RGB;
    format = GL_RGB;
    type = GL_UNSIGNED_BYTE;
    break;

  case UncompressedImage::Format::RGBA:
    internal_format = GL_RGBA;
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    break;

  case UncompressedImage::Format::INVALID:
    return nullptr;

#ifdef __OPTIMIZE__
  default:
    gcc_unreachable();
#endif
  }


// SDL_GLContext ctx = SDL_GL_GetCurrentContext();
// if (ctx == NULL) {
//     LogFormat("QWEQWE Kein gültiger OpenGL Kontext aktiv!");
// } else {
//     LogFormat("ASDASD OpenGL Kontext ist aktiv.");
// }

  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("JJJbefore OpenGL error 0x%X", err0);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  					GLenum err = glGetError();
if (err != GL_NO_ERROR) {
  LogFormat("JJJ OpenGL error 0x%X", err);
}
  return new GLTexture(internal_format, image.GetSize(),
                       format, type, image.GetData(), image.IsFlipped());
}

GLTexture *
ImportAlphaTexture(const UncompressedImage &image)
{
  assert(image.GetFormat() == UncompressedImage::Format::GRAY);

  const GLint internal_format = GL_ALPHA;
  const GLenum format = GL_ALPHA, type = GL_UNSIGNED_BYTE;

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  					GLenum err = glGetError();
if (err != GL_NO_ERROR) {
  LogFormat("KKK OpenGL error 0x%X", err);
}
  return new GLTexture(internal_format, image.GetSize(),
                       format, type, image.GetData(), image.IsFlipped());
}
