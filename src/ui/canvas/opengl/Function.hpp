// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once
#include "LogFile.hpp"
#ifdef USE_EGL
#include "ui/egl/System.hpp"
#elif defined(USE_GLX)

/* kludges to work around namespace collisions with X11 headers */

#define Font X11Font
#define Window X11Window
#define Display X11Display
#include "ui/opengl/System.hpp"
#include <GL/glx.h>

#undef Font
#undef Window
#undef Display

#elif defined(ENABLE_SDL)
#include <SDL_video.h>
#else
#include <dlfcn.h>
#endif

namespace OpenGL {

typedef void (*Function)();

static inline Function
GetProcAddress(const char *name)
{
#ifdef USE_EGL
  Function f = (Function)eglGetProcAddress(name);
  GLenum err0 = glGetError();
  if (err0 != GL_NO_ERROR)
    LogFormat("EMEM OpenGL error 0x%X", err0);
  return f;
#elif defined(USE_GLX)
  Function f = (Function)glXGetProcAddressARB((const GLubyte *)name);
  GLenum err0 = glGetError();
  if (err0 != GL_NO_ERROR)
    LogFormat("EMEM OpenGL error 0x%X", err0);
  return f;
#elif defined(ENABLE_SDL)
  Function f = (Function)SDL_GL_GetProcAddress(name);
  GLenum err0 = glGetError();
  if (err0 != GL_NO_ERROR)
    LogFormat("MM SDL error 0x%X", err0);
  return f;
#else
  Function f = (Function)dlsym(RTLD_DEFAULT, name);
  GLenum err0 = glGetError();
  if (err0 != GL_NO_ERROR)
    LogFormat("EMEM OpenGL error 0x%X", err0);
  return f;
#endif
}

} // namespace OpenGL
