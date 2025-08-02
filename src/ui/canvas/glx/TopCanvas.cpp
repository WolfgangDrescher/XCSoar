// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ui/canvas/custom/TopCanvas.hpp"
#include "ui/canvas/opengl/Globals.hpp"
#include "ui/display/Display.hpp"
#include "ui/dim/Size.hpp"

#include <stdexcept>

TopCanvas::TopCanvas(UI::Display &_display,
                     X11Window x_window)
  :display(_display)
{
  glx_window = glXCreateWindow(display.GetXDisplay(), _display.GetFBConfig(),
                               x_window, nullptr);
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("BVABVA OpenGL error 0x%X", err0);
  XSync(display.GetXDisplay(), false);

  if (!glXMakeContextCurrent(display.GetXDisplay(), glx_window, glx_window,
                             display.GetGLXContext()))
    throw std::runtime_error("Failed to attach GLX context to GLX window");
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("BVBBVB OpenGL error 0x%X", err1);

  const PixelSize effective_size = GetNativeSize();
  if (effective_size.width == 0 || effective_size.height == 0)
    throw std::runtime_error("Failed to query GLX drawable size");

  SetupViewport(effective_size);
}

TopCanvas::~TopCanvas() noexcept
{
  glXDestroyWindow(display.GetXDisplay(), glx_window);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("BVCBVC OpenGL error 0x%X", err0);
}

PixelSize
TopCanvas::GetNativeSize() const noexcept
{
  unsigned w = 0, h = 0;
  glXQueryDrawable(display.GetXDisplay(), glx_window, GLX_WIDTH, &w);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("BVBV OpenGL error 0x%X", err0);
  glXQueryDrawable(display.GetXDisplay(), glx_window, GLX_HEIGHT, &h);
  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("BWBW OpenGL error 0x%X", err1);
  if (w <= 0 || h <= 0)
    return PixelSize(0, 0);

  return PixelSize(w, h);
}

void
TopCanvas::Flip()
{
  glXSwapBuffers(display.GetXDisplay(), glx_window);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("BXBX OpenGL error 0x%X", err0);
}
