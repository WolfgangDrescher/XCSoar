// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Loop.hpp"
#include "Queue.hpp"
#include "Event.hpp"
#include "ui/event/Idle.hpp"
#include "ui/window/TopWindow.hpp"
#include "LogFile.hpp"

#include "ui/canvas/custom/TopCanvas.hpp"
#include "ui/canvas/Features.hpp"
#include "ui/dim/Size.hpp"
#include "lib/fmt/RuntimeError.hxx"
#include "Asset.hpp"
#include "ui/opengl/System.hpp"

#ifdef ENABLE_OPENGL
#include "ui/dim/Rect.hpp"
#include "ui/canvas/opengl/Init.hpp"
#include "Math/Point2D.hpp"
#include "LogFile.hpp"
#else
#include "ui/canvas/memory/Export.hpp"
#include "ui/canvas/Canvas.hpp"
#endif

#ifdef DITHER
#include "ui/canvas/memory/Dither.hpp"
#endif

#include <SDL_platform.h>
#include <SDL_video.h>
#include <SDL_hints.h>
#ifdef USE_MEMORY_CANVAS
#include <SDL_render.h>
#endif
#if defined(__MACOSX__) && __MACOSX__
#include <SDL_syswm.h>
#import <AppKit/AppKit.h>
#include <alloca.h>
#endif

#include <cassert>

namespace UI {

bool
EventLoop::Get(Event &event)
{
  if (bulk) {
    if (queue.Pop(event))
      return true;

    /* that was the last event for now, refresh the screen now */
    if (top_window != nullptr)
      top_window->Refresh();

    bulk = false;
  }

  if (queue.Wait(event)) {
    bulk = true;
    return true;
  }

  return false;
}

void
EventLoop::Dispatch(const Event &_event)
{
	GLenum err7 = glGetError(); if (err7 != GL_NO_ERROR) LogFormat("GGGG SDL error 0x%X", err7);
  const SDL_Event &event = _event.event;
GLenum err6 = glGetError(); if (err6 != GL_NO_ERROR) LogFormat("FFFF SDL error 0x%X", err6);
  if (event.type == EVENT_CALLBACK) {
    Callback callback = (Callback)event.user.data1;
    callback(event.user.data2);
  } else if (top_window != nullptr) {
	GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("EEEE SDL error 0x%X", err5);
    if (top_window->OnEvent(event) &&
        _event.IsUserInput())
      ResetUserIdle();
  }
}

} // namespace UI
