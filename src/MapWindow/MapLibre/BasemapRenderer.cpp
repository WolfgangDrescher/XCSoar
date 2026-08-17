// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "BasemapRenderer.hpp"
#include "CameraBridge.hpp"
#include "GlStateGuard.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Projection/WindowProjection.hpp"
#include "system/Path.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/opengl/Function.hpp"
#include "ui/canvas/opengl/Globals.hpp"
#include "ui/canvas/opengl/Shaders.hpp"
#include "ui/canvas/opengl/Program.hpp"
#include "ui/canvas/opengl/Texture.hpp"
#include "ui/canvas/opengl/FBO.hpp"
#include "ui/event/PeriodicTimer.hpp"

/* the GLX/X11 headers (via ui/canvas/opengl/Function.hpp) leak
   macros that collide with identifiers in the MapLibre headers */
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef Always
#undef Always
#endif
#ifdef Success
#undef Success
#endif

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gl/renderable_resource.hpp>
#include <mbgl/gl/renderer_backend.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/map/map_observer.hpp>
#include <mbgl/map/map_options.hpp>
#include <mbgl/renderer/renderer.hpp>
#include <mbgl/renderer/renderer_frontend.hpp>
#include <mbgl/storage/resource_options.hpp>
#include <mbgl/style/style.hpp>
#include <mbgl/util/run_loop.hpp>

#include <cassert>

using namespace std::chrono_literals;

namespace MapLibre {

/**
 * How often the MapLibre run loop is polled for completed background
 * work (tile downloads, parsed styles, ...) while no rendering
 * happens.
 */
static constexpr auto PUMP_INTERVAL = 100ms;

/**
 * A MapLibre renderer backend that shares XCSoar's OpenGL context and
 * renders into a framebuffer object owned by this class.
 */
class Backend final
  : public mbgl::gl::RendererBackend, public mbgl::gfx::Renderable {

  class Resource final : public mbgl::gl::RenderableResource {
    Backend &backend;

  public:
    explicit Resource(Backend &_backend) noexcept:backend(_backend) {}

    void bind() override {
      backend.setFramebufferBinding(backend.framebuffer);
      backend.setViewport(0, 0, backend.getSize());
    }
  };

  GLuint framebuffer = 0;

  /** the color attachment; flipped=true because FBO contents are
      upside down relative to screen coordinates */
  std::unique_ptr<GLTexture> texture;

  GLuint depth_stencil_buffer = 0;

public:
  explicit Backend(mbgl::Size size) noexcept
    :mbgl::gl::RendererBackend(mbgl::gfx::ContextMode::Shared),
     mbgl::gfx::Renderable(size, std::make_unique<Resource>(*this))
  {
    CreateFramebuffer();
  }

  ~Backend() noexcept override {
    DestroyFramebuffer();
  }

  GLTexture &GetTexture() noexcept {
    assert(texture);
    return *texture;
  }

  void SetSize(mbgl::Size new_size) noexcept {
    if (new_size == size)
      return;

    size = new_size;
    DestroyFramebuffer();
    CreateFramebuffer();
  }

  /* virtual methods from mbgl::gfx::RendererBackend */
  mbgl::gfx::Renderable &getDefaultRenderable() override {
    return *this;
  }

protected:
  void activate() override {
    /* the context is always current on the UI thread */
  }

  void deactivate() override {
  }

  /* virtual methods from mbgl::gl::RendererBackend */
  mbgl::gl::ProcAddress getExtensionFunctionPointer(const char *name) override {
    return (mbgl::gl::ProcAddress)OpenGL::GetProcAddress(name);
  }

  void updateAssumedState() override {
    assumeFramebufferBinding(ImplicitFramebufferBinding);
    assumeViewport(0, 0, size);
  }

private:
  void CreateFramebuffer() noexcept {
    texture = std::make_unique<GLTexture>(GL_RGBA,
                                          PixelSize{size.width, size.height},
                                          GL_RGBA, GL_UNSIGNED_BYTE,
                                          true /* flipped */);

    const PixelSize allocated = texture->GetAllocatedSize();

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    texture->AttachFramebuffer(FBO::COLOR_ATTACHMENT0);

    /* MapLibre needs both a depth and a stencil buffer */
    glGenRenderbuffers(1, &depth_stencil_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_buffer);

    if (OpenGL::render_buffer_depth_stencil != GL_NONE) {
      glRenderbufferStorage(GL_RENDERBUFFER,
                            OpenGL::render_buffer_depth_stencil,
                            allocated.width, allocated.height);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, FBO::DEPTH_ATTACHMENT,
                                GL_RENDERBUFFER, depth_stencil_buffer);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, FBO::STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, depth_stencil_buffer);
    } else {
      /* no packed depth+stencil support: at least attach a stencil
         buffer, which MapLibre strictly requires for polygon
         clipping */
      glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                            allocated.width, allocated.height);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, FBO::STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, depth_stencil_buffer);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void DestroyFramebuffer() noexcept {
    if (framebuffer != 0) {
      glDeleteFramebuffers(1, &framebuffer);
      framebuffer = 0;
    }

    if (depth_stencil_buffer != 0) {
      glDeleteRenderbuffers(1, &depth_stencil_buffer);
      depth_stencil_buffer = 0;
    }

    texture.reset();
  }
};

/**
 * The bridge between mbgl::Map and XCSoar's on-demand rendering: it
 * stores the latest update parameters and requests a redraw of the
 * map window; the actual rendering happens later inside
 * BasemapRenderer::Draw().
 */
class Frontend final : public mbgl::RendererFrontend {
  Backend &backend;

  const std::function<void()> invalidate;

  std::unique_ptr<mbgl::Renderer> renderer;

  std::shared_ptr<mbgl::UpdateParameters> update_parameters;

public:
  Frontend(Backend &_backend, float pixel_ratio,
           std::function<void()> _invalidate) noexcept
    :backend(_backend), invalidate(std::move(_invalidate)),
     renderer(std::make_unique<mbgl::Renderer>(backend, pixel_ratio)) {}

  ~Frontend() noexcept override {
    /* the renderer must release its OpenGL resources with an active
       backend scope */
    mbgl::gfx::BackendScope guard{backend};
    renderer.reset();
  }

  void Render() {
    if (!update_parameters)
      return;

    mbgl::gfx::BackendScope guard{backend,
                                  mbgl::gfx::BackendScope::ScopeType::Implicit};

    /* hold a local reference because a callback invoked during
       render() might replace #update_parameters */
    auto parameters = update_parameters;
    renderer->render(parameters);
  }

  /* virtual methods from mbgl::RendererFrontend */
  void reset() override {
    if (renderer) {
      mbgl::gfx::BackendScope guard{backend};
      renderer.reset();
    }
  }

  void setObserver(mbgl::RendererObserver &observer) override {
    assert(renderer);
    renderer->setObserver(&observer);
  }

  void update(std::shared_ptr<mbgl::UpdateParameters> parameters) override {
    update_parameters = std::move(parameters);
    invalidate();
  }

  const mbgl::TaggedScheduler &getThreadPool() const override {
    return backend.getThreadPool();
  }
};

/** logs style/resource problems instead of failing silently */
class Observer final : public mbgl::MapObserver {
public:
  void onDidFailLoadingMap(mbgl::MapLoadError,
                           const std::string &message) override {
    LogFormat("MapLibre: failed to load map: %s", message.c_str());
  }
};

struct BasemapRenderer::Impl {
  /* note: the member order defines the construction/destruction
     order and matters: the run loop must exist first, the map must
     be destroyed before frontend and backend */

  /** MapLibre's callback dispatcher for the UI thread */
  mbgl::util::RunLoop run_loop;

  Backend backend;

  Frontend frontend;

  Observer observer;

  mbgl::Map map;

  /** polls #run_loop while background work (tile loading) is in
      progress */
  UI::PeriodicTimer pump_timer{[this]{ run_loop.runOnce(); }};

  /** the camera of the last Draw() call, to avoid re-triggering
      mbgl::Map updates when nothing changed */
  Camera last_camera{};
  bool camera_valid = false;

  Impl(const char *style_url, float pixel_ratio,
       std::function<void()> invalidate) noexcept
    :backend({1, 1}),
     frontend(backend, pixel_ratio, std::move(invalidate)),
     map(frontend, observer,
         mbgl::MapOptions()
           .withMapMode(mbgl::MapMode::Continuous)
           .withSize(backend.getSize())
           .withPixelRatio(pixel_ratio),
         mbgl::ResourceOptions()
           .withCachePath(MakeCachePath()))
  {
    map.getStyle().loadURL(style_url);
    pump_timer.Schedule(PUMP_INTERVAL);
  }

  static std::string MakeCachePath() noexcept {
    const auto directory = MakeCacheDirectory("maplibre");
    return AllocatedPath::Build(directory, "cache.db").c_str();
  }

  void Draw(Canvas &canvas, const WindowProjection &projection) noexcept;
};

inline void
BasemapRenderer::Impl::Draw([[maybe_unused]] Canvas &canvas,
                            const WindowProjection &projection) noexcept
{
  const auto camera = CameraFromProjection(projection);

  const mbgl::Size viewport{camera.viewport.width, camera.viewport.height};
  if (viewport != backend.getSize()) {
    backend.SetSize(viewport);
    map.setSize(viewport);
  }

  /* only push a new camera into MapLibre when it actually changed,
     to let the render loop settle once all tiles are loaded */
  if (!camera_valid ||
      camera.center != last_camera.center ||
      camera.zoom != last_camera.zoom ||
      camera.bearing != last_camera.bearing) {
    map.jumpTo(mbgl::CameraOptions()
               .withCenter(mbgl::LatLng{camera.center.latitude.Degrees(),
                                        camera.center.longitude.Degrees()})
               .withZoom(camera.zoom)
               .withBearing(camera.bearing.Degrees())
               .withPitch(0.));
    last_camera = camera;
    camera_valid = true;
  }

  /* let MapLibre process finished background work now */
  run_loop.runOnce();

  {
    GlStateGuard guard;
    frontend.Render();
  }

  /* composite the rendered basemap into the map window */
  OpenGL::texture_shader->Use();

  auto &texture = backend.GetTexture();
  texture.Bind();

  const PixelRect rect{PixelPoint{0, 0},
                       PixelSize{viewport.width, viewport.height}};
  texture.Draw(rect, rect);
}

BasemapRenderer::BasemapRenderer(const char *style_url, float pixel_ratio,
                                 std::function<void()> invalidate) noexcept
  :impl(std::make_unique<Impl>(style_url, pixel_ratio,
                               std::move(invalidate))) {}

BasemapRenderer::~BasemapRenderer() noexcept = default;

void
BasemapRenderer::Draw(Canvas &canvas,
                      const WindowProjection &projection) noexcept
{
  impl->Draw(canvas, projection);
}

} // namespace MapLibre
