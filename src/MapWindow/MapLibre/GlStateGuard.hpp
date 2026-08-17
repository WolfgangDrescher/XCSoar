// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/opengl/System.hpp"

namespace MapLibre {

/**
 * Protects XCSoar's global OpenGL state while MapLibre renders.
 *
 * XCSoar's OpenGL canvas is built around a set of implicit baseline
 * assumptions (see OpenGL::SetupContext()): depth/stencil/scissor
 * tests disabled, no bound VAO/VBO, texture unit 0 active, dithering
 * enabled.  MapLibre's renderer tracks its own GL state and leaves
 * arbitrary bindings behind.  Constructing this object saves the
 * bindings that must survive (framebuffer, viewport); the destructor
 * restores them and re-establishes XCSoar's baseline state.
 */
class GlStateGuard {
  GLint framebuffer;
  GLint viewport[4];

public:
  GlStateGuard() noexcept;
  ~GlStateGuard() noexcept;

  GlStateGuard(const GlStateGuard &) = delete;
  GlStateGuard &operator=(const GlStateGuard &) = delete;
};

} // namespace MapLibre
