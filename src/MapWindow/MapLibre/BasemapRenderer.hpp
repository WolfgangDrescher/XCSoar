// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <functional>
#include <memory>

class Canvas;
class WindowProjection;

namespace MapLibre {

/**
 * Renders a MapLibre basemap (terrain, topography, raster/vector
 * tiles) below XCSoar's tactical Canvas overlays.
 *
 * The MapLibre map is rendered off-screen into a framebuffer object
 * whose texture is then composited into the current canvas, so
 * MapLibre's OpenGL state never leaks into XCSoar's (see
 * #GlStateGuard).  The camera follows XCSoar's #WindowProjection via
 * CameraFromProjection().
 *
 * Instances must be created, used and destroyed on the UI/render
 * thread (the thread owning the OpenGL context).  Only one instance
 * may exist at a time because it owns the thread's MapLibre run
 * loop.
 *
 * This class is only available in ENABLE_MAPLIBRE builds; the header
 * itself is free of MapLibre includes.
 */
class BasemapRenderer final {
  struct Impl;
  std::unique_ptr<Impl> impl;

public:
  /**
   * @param style_url a MapLibre style URL (e.g. "https://..." or
   * "file://...")
   * @param pixel_ratio ratio between physical pixels and MapLibre's
   * logical pixels (affects label/symbol sizes)
   * @param invalidate called (on the UI thread) whenever the basemap
   * has new content and the map window should be redrawn
   */
  BasemapRenderer(const char *style_url, float pixel_ratio,
                  std::function<void()> invalidate) noexcept;
  ~BasemapRenderer() noexcept;

  BasemapRenderer(const BasemapRenderer &) = delete;
  BasemapRenderer &operator=(const BasemapRenderer &) = delete;

  /**
   * Render the basemap for the given projection and composite it
   * into the canvas.  Must be called with the OpenGL context
   * current, i.e. from the MapWindow render pass.
   */
  void Draw(Canvas &canvas, const WindowProjection &projection) noexcept;
};

} // namespace MapLibre
