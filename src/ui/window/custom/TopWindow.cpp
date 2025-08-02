// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "../TopWindow.hpp"
#include "ui/canvas/Features.hpp" // for DRAW_MOUSE_CURSOR
#include "ui/canvas/custom/TopCanvas.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/event/Queue.hpp"
#include "ui/event/Globals.hpp"
#include "Hardware/CPU.hpp"

#ifdef ANDROID
#include "Android/Main.hpp"
#include "Android/NativeView.hpp"
#include "ui/event/android/Loop.hpp"
#include "util/ScopeExit.hxx"
#elif defined(ENABLE_SDL)
#include "ui/event/sdl/Event.hpp"
#include "ui/event/sdl/Loop.hpp"
#else
#include "ui/event/poll/Loop.hpp"
#include "ui/event/shared/Event.hpp"
#endif

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Dynamic.hpp" // for GLExt::discard_framebuffer
#endif

#ifdef DRAW_MOUSE_CURSOR
#include "Screen/Layout.hpp"
#endif

namespace UI {

TopWindow::~TopWindow() noexcept
{
#ifdef ANDROID
  native_view->SetPointer(Java::GetEnv(), nullptr);
#endif

  delete screen;
}

void
TopWindow::Create([[maybe_unused]] const TCHAR *text, PixelSize size,
                  TopWindowStyle style)
{
  invalidated = true;

#if defined(USE_X11) || defined(USE_WAYLAND) || defined(ENABLE_SDL)
  CreateNative(text, size, style);
#endif

  delete screen;
  screen = nullptr;

#ifdef ENABLE_SDL
  screen = new TopCanvas(display, window);
#elif defined(USE_GLX)
  screen = new TopCanvas(display, x_window);
#elif defined(USE_X11)
  screen = new TopCanvas(display, x_window);
#elif defined(USE_WAYLAND)
  screen = new TopCanvas(display, native_window);
#elif defined(USE_VFB)
  screen = new TopCanvas(display, size);
#else
  screen = new TopCanvas(display);
#endif

#ifdef SOFTWARE_ROTATE_DISPLAY
  size = screen->SetDisplayOrientation(style.GetInitialOrientation());
#elif defined(USE_MEMORY_CANVAS)
  size = screen->GetSize();
#endif
  ContainerWindow::Create(nullptr, PixelRect{size}, style);
}

#ifdef SOFTWARE_ROTATE_DISPLAY

void
TopWindow::SetDisplayOrientation(DisplayOrientation orientation) noexcept
{
  assert(screen != nullptr);

  Resize(screen->SetDisplayOrientation(orientation));
}

#endif

void
TopWindow::CancelMode() noexcept
{
  OnCancelMode();
}

void
TopWindow::Invalidate() noexcept
{
  invalidated = true;
}

#ifdef DRAW_MOUSE_CURSOR

inline void
TopWindow::DrawMouseCursor(Canvas &canvas) noexcept
{
  const auto m = event_queue->GetMousePosition();
  const int shortDistance = Layout::Scale(cursor_size * 4);
  const int longDistance = Layout::Scale(cursor_size * 6);

  const BulkPixelPoint p[] = {
    { m.x, m.y },
    { m.x + shortDistance, m.y + shortDistance },
    { m.x, m.y + longDistance },
  };

  if (invert_cursor_colors) {
    canvas.SelectWhitePen(cursor_size);
    canvas.SelectBlackBrush();
  } else {
    canvas.SelectBlackPen(cursor_size);
    canvas.SelectWhiteBrush();
  }
  canvas.DrawTriangleFan(p, std::size(p));
}

#endif

void
TopWindow::Expose() noexcept
{
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("8888 SDL error 0x%X", err1);
#ifdef HAVE_CPU_FREQUENCY
  const ScopeLockCPU cpu;
#endif

  if (auto canvas = screen->Lock(); canvas.IsDefined()) {
    OnPaint(canvas);
GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("9999 SDL error 0x%X", err1);
#ifdef DRAW_MOUSE_CURSOR
    if (std::chrono::steady_clock::now() < cursor_visible_until) {
		GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("10 SDL error 0x%X", err2);
		DrawMouseCursor(canvas);
		GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("11 SDL error 0x%X", err3);
	}
#endif
GLenum err4 = glGetError(); if (err4 != GL_NO_ERROR) LogFormat("12 SDL error 0x%X", err4);
    screen->Unlock();
	GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("13 SDL error 0x%X", err5);
  }

  GLenum err6 = glGetError(); if (err6 != GL_NO_ERROR) LogFormat("14 SDL error 0x%X", err6);
  screen->Flip();
  GLenum err7 = glGetError(); if (err7 != GL_NO_ERROR) LogFormat("15 SDL error 0x%X", err7);

#if defined(ENABLE_OPENGL) && defined(GL_EXT_discard_framebuffer)
  /* tell the GPU that we won't be needing the frame buffer contents
     again which can increase rendering performance; see
     https://registry.khronos.org/OpenGL/extensions/EXT/EXT_discard_framebuffer.txt */
  if (GLExt::discard_framebuffer != nullptr) {
	GLenum err8 = glGetError(); if (err8 != GL_NO_ERROR) LogFormat("16 SDL error 0x%X", err8);
    static constexpr GLenum attachments[3] = {
    //   GL_COLOR_EXT,
    //   GL_DEPTH_EXT,
    //   GL_STENCIL_EXT
 GL_COLOR_ATTACHMENT0,
  GL_DEPTH_ATTACHMENT,
  GL_STENCIL_ATTACHMENT
    };
GLenum err10 = glGetError(); if (err10 != GL_NO_ERROR) LogFormat("17 SDL error 0x%X", err10);
    GLExt::discard_framebuffer(GL_FRAMEBUFFER, std::size(attachments),
                               attachments);
GLenum err9 = glGetError(); if (err9 != GL_NO_ERROR) LogFormat("16 SDL error 0x%X", err9);
  }
#endif
}

void
TopWindow::Refresh() noexcept
{
	GLenum errK = glGetError(); if (errK != GL_NO_ERROR) LogFormat("5555 SDL error 0x%X", errK);
  if (!screen->IsReady())
    /* the application is paused/suspended, and we don't have an
       OpenGL surface - ignore all drawing requests */
    return;

#ifdef USE_X11
  if (!IsVisible())
    /* don't bother to invoke the renderer if we're not visible on the
       X11 display */
    return;
#endif

  if (!invalidated)
    return;

  invalidated = false;
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("6666 SDL error 0x%X", err0);
  Expose();
  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("7777 SDL error 0x%X", err1);
}

bool
TopWindow::OnActivate() noexcept
{
  return false;
}

bool
TopWindow::OnDeactivate() noexcept
{
  return false;
}

bool
TopWindow::OnClose() noexcept
{
  Destroy();
  return true;
}

int
TopWindow::RunEventLoop() noexcept
{
#ifdef ANDROID
  BeginRunning();
  AtScopeExit(this) { EndRunning(); };
#endif

  Refresh();

  EventLoop loop(*event_queue, *this);
  Event event;
  while (IsDefined() && loop.Get(event)) {
	  GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("IIII SDL error 0x%X", err5);
		  loop.Dispatch(event);
  }

  return 0;
}

void
TopWindow::PostQuit() noexcept
{
  event_queue->Quit();
}

} // namespace UI
