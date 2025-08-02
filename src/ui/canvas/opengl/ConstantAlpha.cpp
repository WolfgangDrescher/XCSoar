// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ConstantAlpha.hpp"
#include "Shaders.hpp"
#include "Program.hpp"
#include "Attribute.hpp"
#include "ui/opengl/System.hpp"
#include "LogFile.hpp"
/**
 * Combine texture alpha and constant alpha.
 */
static void
CombineAlpha(float alpha)
{
  glVertexAttrib4f(OpenGL::Attribute::COLOR,
                   1, 1, 1, alpha);
GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DXDX OpenGL error 0x%X", err0);

  OpenGL::combine_texture_shader->Use();
}

ScopeTextureConstantAlpha::ScopeTextureConstantAlpha(bool use_texture_alpha,
                                                     float alpha)
  :enabled(use_texture_alpha || alpha < 1.0f)
{
  OpenGL::texture_shader->Use();

  if (!enabled) {
    /* opaque: use plain GL_REPLACE, avoid the alpha blending
       overhead */
    return;
  }

  glEnable(GL_BLEND);
  GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DXDX OpenGL error 0x%X", err0);

  if (use_texture_alpha) {
    if (alpha >= 1.0f) {
      /* use only texture alpha */

      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("DXDX OpenGL error 0x%X", err1);
    } else {
      /* combine texture alpha and constant alpha */

      CombineAlpha(alpha);
    }
  } else {
    /* use only constant alpha, ignore texture alpha */

    /* tell OpenGL to use our alpha value instead of the texture's */
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("DYDY OpenGL error 0x%X", err3);
    glBlendColor(0, 0, 0, alpha);
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("DYDY OpenGL error 0x%X", err2);
  }
  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("DYDY OpenGL error 0x%X", err1);
}

ScopeTextureConstantAlpha::~ScopeTextureConstantAlpha()
{
  if (enabled)
    glDisable(GL_BLEND);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("DZDZ OpenGL error 0x%X", err0);

  /* restore default shader */
  OpenGL::solid_shader->Use();
}
