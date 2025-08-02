// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/opengl/System.hpp"
#include "LogFile.hpp"
/**
 * This class represents an OpenGL 3.0 / ES2.0 shader.
 */
class GLShader {
  const GLuint id;

public:
  explicit GLShader(GLenum type) noexcept:id(glCreateShader(type)) {
	LogFormat("EPEP SAFTY CHECK");
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EPEP OpenGL error 0x%X", err0);
  }

  GLShader(const GLShader &) = delete;
  GLShader &operator=(const GLShader &) = delete;

  ~GLShader() noexcept {
    glDeleteShader(id);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EQEQ OpenGL error 0x%X", err0);
  }

  GLuint GetId() const noexcept {
    return id;
  }

  void Source(const char *_source) noexcept {
    const GLchar *source = (const GLchar *)_source;
    glShaderSource(id, 1, &source, nullptr);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("ERER OpenGL error 0x%X", err0);
  }

  void Compile() noexcept {
    glCompileShader(id);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("ESES OpenGL error 0x%X", err0);
  }

  [[gnu::pure]]
  GLint GetCompileStatus() const noexcept {
    GLint status;
    glGetShaderiv(id, GL_COMPILE_STATUS, &status);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("ETET OpenGL error 0x%X", err0);
    return status;
  }

  [[gnu::pure]]
  GLint GetInfoLogLength() const noexcept {
    GLint length;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EUEU OpenGL error 0x%X", err0);
    return length;
  }

  void GetInfoLog(char *buffer, GLsizei max_size) noexcept {
    glGetShaderInfoLog(id, max_size, nullptr, (GLchar *)buffer);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EVEV OpenGL error 0x%X", err0);
  }
};

/**
 * This class represents an OpenGL 3.0 / ES2.0 program.
 */
class GLProgram {
  const GLuint id;

public:
  GLProgram() noexcept:id(glCreateProgram()) {
	LogFormat("EWEW CHECK");
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EWEW OpenGL error 0x%X", err0);
  }

  GLProgram(const GLProgram &) = delete;
  GLProgram &operator=(const GLProgram &) = delete;

  ~GLProgram() noexcept {
    glDeleteProgram(id);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EXEX OpenGL error 0x%X", err0);
  }

  GLuint GetId() const noexcept {
    return id;
  }

  void AttachShader(const GLShader &shader) noexcept {
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("EYEYbefore OpenGL error 0x%X", err1);
    glAttachShader(id, shader.GetId());
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EYEY OpenGL error 0x%X", err0);
  }

  void Link() noexcept {
    glLinkProgram(id);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("EZEZ OpenGL error 0x%X", err0);
  }

  [[gnu::pure]]
  GLint GetLinkStatus() const noexcept {
    GLint status;
    glGetProgramiv(id, GL_LINK_STATUS, &status);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FAFA OpenGL error 0x%X", err0);
    return status;
  }

  [[gnu::pure]]
  GLint GetInfoLogLength() const noexcept {
    GLint length;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FBFB OpenGL error 0x%X", err0);
    return length;
  }

  void GetInfoLog(char *buffer, GLsizei max_size) noexcept {
    glGetProgramInfoLog(id, max_size, nullptr, (GLchar *)buffer);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FCFC OpenGL error 0x%X", err0);
  }

  void Validate() noexcept {
    glValidateProgram(id);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FDFD OpenGL error 0x%X", err0);
  }

  void Use() noexcept {
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FEFEbefore OpenGL error 0x%X", err0);
    glUseProgram(GetId());
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("FEFE OpenGL error 0x%X", err1);
  }

  [[gnu::pure]]
  GLint GetUniformLocation(const char *name) const noexcept {
	GLint a = glGetUniformLocation(id, (const GLchar *)name);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FGFG OpenGL error 0x%X", err0);
    return a;
  }

  [[gnu::pure]]
  GLint GetAttribLocation(const char *name) const noexcept {
    GLint a = glGetAttribLocation(id, (const GLchar *)name);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FHFH OpenGL error 0x%X", err0);
	return a;
  }

  void BindAttribLocation(GLuint index, const char *name) noexcept {
    glBindAttribLocation(id, index, (const GLchar *)name);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("FIFI OpenGL error 0x%X", err0);
  }
};
