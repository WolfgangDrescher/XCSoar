// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/opengl/SystemExt.hpp"
#include "LogFile.hpp"
#if (defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define GL_UNBIND_FRAMEBUFFER 1
#define GL_UNBIND_RENDERBUFFER 1
#else 
#define GL_UNBIND_FRAMEBUFFER 0
#define GL_UNBIND_RENDERBUFFER 0
#endif

/**
 * Support for OpenGL framebuffer objects (GL_*_framebuffer_object).
 */
namespace FBO {

static constexpr GLenum RENDERBUFFER = GL_RENDERBUFFER;
static constexpr GLenum FRAMEBUFFER = GL_FRAMEBUFFER;
static constexpr GLenum COLOR_ATTACHMENT0 = GL_COLOR_ATTACHMENT0;
static constexpr GLenum DEPTH_ATTACHMENT = GL_DEPTH_ATTACHMENT;
static constexpr GLenum STENCIL_ATTACHMENT = GL_STENCIL_ATTACHMENT;

#ifdef GL_DEPTH_STENCIL
static constexpr GLenum DEPTH_STENCIL = GL_DEPTH_STENCIL;
#elif defined(GL_DEPTH_STENCIL_EXT)
static constexpr GLenum DEPTH_STENCIL = GL_DEPTH_STENCIL_EXT;
#elif defined(GL_DEPTH_STENCIL_NV)
static constexpr GLenum DEPTH_STENCIL = GL_DEPTH_STENCIL_NV;
#elif defined(GL_DEPTH_STENCIL_OES)
static constexpr GLenum DEPTH_STENCIL = GL_DEPTH_STENCIL_OES;
#else
#error No GL_DEPTH_STENCIL found
#endif

static inline void
BindRenderbuffer(GLenum target, GLuint renderbuffer) noexcept
{
  glBindRenderbuffer(target, renderbuffer);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("ECEC OpenGL error 0x%X", err0);
}

static inline void
DeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) noexcept
{
  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("EDEDbefore OpenGL error 0x%X", err1);
  glDeleteRenderbuffers(n, renderbuffers);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EDED OpenGL error 0x%X", err0);
}

static inline void
GenRenderbuffers(GLsizei n, GLuint *renderbuffers) noexcept
{
  glGenRenderbuffers(n, renderbuffers);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EFEF OpenGL error 0x%X", err0);
}

static inline void
RenderbufferStorage(GLenum target, GLenum internalformat,
                    GLsizei width, GLsizei height) noexcept
{
  glRenderbufferStorage(target, internalformat, width, height);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EGEG OpenGL error 0x%X", err0);
}

static inline void
BindFramebuffer(GLenum target, GLuint framebuffer) noexcept
{
  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("EHEHbefore OpenGL error 0x%X", err1);
  glBindFramebuffer(target, framebuffer);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EHEH OpenGL error 0x%X", err0);
}

static inline void
DeleteFramebuffers(GLsizei n, const GLuint *framebuffers) noexcept
{
  glDeleteFramebuffers(n, framebuffers);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EIEI OpenGL error 0x%X", err0);
}

static inline void
GenFramebuffers(GLsizei n, GLuint *framebuffers) noexcept
{
  glGenFramebuffers(n, framebuffers);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EJEJ OpenGL error 0x%X", err0);
}

static inline void
FramebufferRenderbuffer(GLenum target, GLenum attachment,
                        GLenum renderbuffertarget,
                        GLuint renderbuffer) noexcept
{
  glFramebufferRenderbuffer(target, attachment,
                            renderbuffertarget, renderbuffer);
							GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EKEK OpenGL error 0x%X", err0);
}

static inline void
FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                     GLuint texture, GLint level) noexcept
{
  glFramebufferTexture2D(target, attachment, textarget, texture, level);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("ELEL OpenGL error 0x%X", err0);
}

} // namespace OpenGL
