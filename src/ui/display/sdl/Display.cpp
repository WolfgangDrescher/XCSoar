// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Display.hpp"
#include "lib/fmt/RuntimeError.hxx"
#include "Asset.hpp"
#include "ui/opengl/System.hpp"
#include <SDL.h>
#include <SDL_hints.h>
#include "LogFile.hpp"
namespace SDL {

Display::Display()
{
  Uint32 flags = SDL_INIT_VIDEO;
  if (!IsKobo())
    flags |= SDL_INIT_AUDIO;

  if (::SDL_Init(flags) != 0)
    throw FmtRuntimeError("SDL_Init() has failed: {}", ::SDL_GetError());

#ifdef ENABLE_OPENGL
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("AA SDL error 0x%X", err0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  GLenum err2 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("BB SDL error 0x%X", err2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("CC SDL error 0x%X", err3);
#endif

  // Keep screen on (works on iOS, and maybe for other platforms)
  SDL_SetHint(SDL_HINT_IDLE_TIMER_DISABLED, "1");

  if (HasTouchScreen())
    SDL_ShowCursor (SDL_FALSE);

#if defined(ENABLE_OPENGL)
  ::SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  GLenum err4 = glGetError(); if (err4 != GL_NO_ERROR) LogFormat("DD SDL error 0x%X", err4);
  ::SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("EE SDL error 0x%X", err5);
#endif
}

Display::~Display() noexcept
{
  ::SDL_Quit();
}

} // namespace SDL
