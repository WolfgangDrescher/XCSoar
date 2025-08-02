// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/opengl/System.hpp"
#include "ui/opengl/Types.hpp"
#include "Math/Point2D.hpp"
#include "Attribute.hpp"
#include "LogFile.hpp"

struct BulkPixelPoint;
struct ExactPixelPoint;

struct ScopeVertexPointer {
  ScopeVertexPointer() noexcept {
    glEnableVertexAttribArray(OpenGL::Attribute::POSITION);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GAGA OpenGL error 0x%X", err0);
  }

  ~ScopeVertexPointer() noexcept {
    glDisableVertexAttribArray(OpenGL::Attribute::POSITION);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GBGB OpenGL error 0x%X", err0);
  }

  ScopeVertexPointer(GLenum type, const void *p) noexcept {
    glEnableVertexAttribArray(OpenGL::Attribute::POSITION);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GCGC OpenGL error 0x%X", err0);
    Update(type, p);
  }

  template<typename T>
  ScopeVertexPointer(const T *p) noexcept {
    glEnableVertexAttribArray(OpenGL::Attribute::POSITION);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GDGD OpenGL error 0x%X", err0);
    Update(p);
  }

  void Update(GLenum type, GLsizei stride, const void *p) noexcept {
    glVertexAttribPointer(OpenGL::Attribute::POSITION, 2, type,
                          GL_FALSE, stride, p);
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("GEGE OpenGL error 0x%X", err0);
  }

  void Update(GLenum type, const void *p) noexcept {
    Update(type, 0, p);
  }

  void Update(const BulkPixelPoint *p) noexcept {
    Update(GL_VALUE, p);
  }

  void Update(const ExactPixelPoint *p) noexcept {
    Update(GL_EXACT, p);
  }

  void Update(const FloatPoint2D *p) noexcept {
    Update(GL_FLOAT, p);
  }
};
