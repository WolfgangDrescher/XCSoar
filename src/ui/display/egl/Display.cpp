// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Display.hpp"
#include "ConfigChooser.hpp"
#include "lib/fmt/RuntimeError.hxx"
#include "LogFile.hpp"

#include <cassert>

namespace EGL {

Display::Display(EGLNativeDisplayType native_display)
{
  InitDisplay(native_display);
  CreateContext();
}

Display::~Display() noexcept
{
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("GKGK OpenGL error 0x%X", err1);


  if (dummy_surface != EGL_NO_SURFACE)
    eglDestroySurface(display, dummy_surface);
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("GKGK OpenGL error 0x%X", err2);

  eglDestroyContext(display, context);
  GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("GKGK OpenGL error 0x%X", err3);
  eglTerminate(display);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GKGK OpenGL error 0x%X", err0);
}

[[gnu::pure]]
static int
GetConfigAttrib(EGLDisplay display, EGLConfig config,
                int attribute, int default_value) noexcept
{
  int value;
  int a = eglGetConfigAttrib(display, config, attribute, &value)
    ? value
    : default_value;
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GLGL OpenGL error 0x%X", err0);
	return a;
}

inline void
Display::InitDisplay(EGLNativeDisplayType native_display)
{
  assert(display == EGL_NO_DISPLAY);

  display = eglGetDisplay(native_display);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GLGL OpenGL error 0x%X", err0);
  if (display == EGL_NO_DISPLAY)
    throw std::runtime_error("eglGetDisplay(EGL_DEFAULT_DISPLAY) failed");

  if (!eglInitialize(display, nullptr, nullptr))
    throw std::runtime_error("eglInitialize() failed");

  if (const char *s = eglQueryString(display, EGL_VENDOR))
    LogFormat("EGL vendor: %s", s);

  if (const char *s = eglQueryString(display, EGL_VERSION))
    LogFormat("EGL version: %s", s);

  if (const char *s = eglQueryString(display, EGL_EXTENSIONS))
    LogFormat("EGL extensions: %s", s);

  if (!eglBindAPI(EGL_OPENGL_ES_API))
    throw std::runtime_error("eglBindAPI() failed");

  chosen_config = EGL::ChooseConfig(display);

GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GMGM OpenGL error 0x%X", err0);

  LogFormat("EGL config: RGB=%d/%d/%d alpha=%d depth=%d stencil=%d",
            GetConfigAttrib(display, chosen_config, EGL_RED_SIZE, 0),
            GetConfigAttrib(display, chosen_config, EGL_GREEN_SIZE, 0),
            GetConfigAttrib(display, chosen_config, EGL_BLUE_SIZE, 0),
            GetConfigAttrib(display, chosen_config, EGL_ALPHA_SIZE, 0),
            GetConfigAttrib(display, chosen_config, EGL_DEPTH_SIZE, 0),
            GetConfigAttrib(display, chosen_config, EGL_STENCIL_SIZE, 0));
}

inline void
Display::CreateContext()
{
  assert(display != EGL_NO_DISPLAY);
  assert(context == EGL_NO_CONTEXT);

  static constexpr EGLint context_attributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  context = eglCreateContext(display, chosen_config,
                             EGL_NO_CONTEXT, context_attributes);
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GNGN OpenGL error 0x%X", err0);
  if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
    /* some old EGL implemenations do not support EGL_NO_SURFACE
       (error EGL_BAD_MATCH); this kludge uses a dummy 1x1 pbuffer
       surface to work around this */
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GOGO OpenGL error 0x%X", err0);
    static constexpr int pbuffer_attributes[] = {
      EGL_WIDTH, 1,
      EGL_HEIGHT, 1,
      EGL_NONE
    };

    dummy_surface = eglCreatePbufferSurface(display, chosen_config,
                                            pbuffer_attributes);
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GPGP OpenGL error 0x%X", err0);
    if (dummy_surface == EGL_NO_SURFACE)
      throw FmtRuntimeError("eglCreatePbufferSurface() failed: {:#x}", eglGetError());

    MakeCurrent(dummy_surface);
  }
}

EGLSurface
Display::CreateWindowSurface(EGLNativeWindowType native_window)
{
  auto surface = eglCreateWindowSurface(display, chosen_config,
                                        native_window, nullptr);
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GQGQ OpenGL error 0x%X", err0);
  if (surface == EGL_NO_SURFACE)
    throw FmtRuntimeError("eglCreateWindowSurface() failed: {:#x}", eglGetError());

  return surface;
}

void
Display::MakeCurrent(EGLSurface surface)
{
  if (surface == EGL_NO_SURFACE)
    surface = dummy_surface;

  if (!eglMakeCurrent(display, surface, surface, context))
    throw FmtRuntimeError("eglMakeCurrent() failed: {:#x}", eglGetError());
}

} // namespace EGL
