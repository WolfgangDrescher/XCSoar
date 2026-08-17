// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlStateGuard.hpp"
#include "ui/canvas/opengl/Function.hpp"

#include <initializer_list>

namespace MapLibre {

using BindVertexArrayFunction = void (*)(GLuint array);

/**
 * Resolve glBindVertexArray() dynamically: it is not part of GLES
 * 2.0, but MapLibre uses vertex array objects when the driver
 * supports them, and a VAO left bound would corrupt XCSoar's vertex
 * attribute setup.
 */
static BindVertexArrayFunction
ResolveBindVertexArray() noexcept
{
  for (const char *name : {"glBindVertexArray", "glBindVertexArrayOES"})
    if (auto f = OpenGL::GetProcAddress(name))
      return (BindVertexArrayFunction)f;

  return nullptr;
}

GlStateGuard::GlStateGuard() noexcept
{
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_VIEWPORT, viewport);
}

GlStateGuard::~GlStateGuard() noexcept
{
  /* unbind a possibly bound VAO first: attribute/buffer state below
     must be applied to the default vertex array */
  static const auto bind_vertex_array = ResolveBindVertexArray();
  if (bind_vertex_array != nullptr)
    bind_vertex_array(0);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  /* XCSoar enables the vertex attribute arrays it needs per draw
     call; stray enabled arrays with stale pointers would crash */
  for (GLuint i = 0; i < 8; ++i)
    glDisableVertexAttribArray(i);

  glActiveTexture(GL_TEXTURE0);
  glUseProgram(0);

  /* re-establish the baseline set up by OpenGL::SetupContext() */
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glEnable(GL_DITHER);

  glDepthMask(GL_TRUE);
  glStencilMask(~GLuint(0));
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

} // namespace MapLibre
