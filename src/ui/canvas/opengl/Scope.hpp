// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/opengl/System.hpp"
#include "LogFile.hpp"
/**
 * Enables and auto-disables an OpenGL capability.
 */
template<GLenum cap>
class GLEnable {
public:
  [[nodiscard]]
  GLEnable() noexcept {
    ::glEnable(cap);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FMAFMA OpenGL error 0x%X", err0);
  }

  ~GLEnable() noexcept {
    ::glDisable(cap);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FMBFMB OpenGL error 0x%X", err0);
  }

  GLEnable(const GLEnable &) = delete;
  GLEnable &operator=(const GLEnable &) = delete;
};

class GLBlend : public GLEnable<GL_BLEND> {
public:
  [[nodiscard]]
  GLBlend(GLenum sfactor, GLenum dfactor) noexcept {
    ::glBlendFunc(sfactor, dfactor);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FMFM OpenGL error 0x%X", err0);
  }

  [[nodiscard]]
  GLBlend(GLclampf alpha) noexcept {
    ::glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("FNFN OpenGL error 0x%X", err1);
    ::glBlendColor(0, 0, 0, alpha);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FNFN OpenGL error 0x%X", err0);
  }
};

/**
 * Enable alpha blending with source's alpha value (the most common
 * variant of GL_BLEND).
 */
class ScopeAlphaBlend : GLBlend {
public:
  [[nodiscard]]
  ScopeAlphaBlend() noexcept:GLBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) {}
};

class GLScissor : public GLEnable<GL_SCISSOR_TEST> {
public:
  [[nodiscard]]
  GLScissor(GLint x, GLint y, GLsizei width, GLsizei height) noexcept {
    ::glScissor(x, y, width, height);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FOFO OpenGL error 0x%X", err0);
  }
};
