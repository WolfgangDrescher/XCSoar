// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project
#include "LogFile.hpp"
#ifdef ENABLE_OPENGL

#include "AirspaceRenderer.hpp"
#include "AirspaceRendererSettings.hpp"
#include "Projection/WindowProjection.hpp"
#include "ui/canvas/Canvas.hpp"
#include "MapWindow/MapCanvas.hpp"
#include "Look/AirspaceLook.hpp"
#include "Airspace/Airspaces.hpp"
#include "Airspace/AirspacePolygon.hpp"
#include "Airspace/AirspaceCircle.hpp"
#include "Airspace/AirspaceWarningCopy.hpp"
#include "Engine/Airspace/Predicate/AirspacePredicate.hpp"
#include "ui/canvas/opengl/Scope.hpp"

class AirspaceVisitorRenderer final
  : protected MapCanvas
{
  const AirspaceLook &look;
  const AirspaceWarningCopy &warning_manager;
  const AirspaceRendererSettings &settings;

public:
  AirspaceVisitorRenderer(Canvas &_canvas, const WindowProjection &_projection,
                          const AirspaceLook &_look,
                          const AirspaceWarningCopy &_warnings,
                          const AirspaceRendererSettings &_settings)
    :MapCanvas(_canvas, _projection,
               _projection.GetScreenBounds().Scale(1.1)),
     look(_look), warning_manager(_warnings), settings(_settings)
  {
    glStencilMask(0xff);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("AEAE OpenGL error 0x%X", err0);
    glClear(GL_STENCIL_BUFFER_BIT);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("AFAF OpenGL error 0x%X", err1);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("AGAG OpenGL error 0x%X", err2);

  }

  ~AirspaceVisitorRenderer() {
    glStencilMask(0xff);
	GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("AHAH OpenGL error 0x%X", err3);

  }

private:
  void VisitCircle(const AirspaceCircle &airspace) {
	AirspaceClass as_type_or_class = settings.classes[airspace.GetTypeOrClass()].display ? airspace.GetTypeOrClass() : airspace.GetClass();
    const AirspaceClassRendererSettings &class_settings =
      settings.classes[as_type_or_class];
    const AirspaceClassLook &class_look = look.classes[as_type_or_class];

    auto screen_center = projection.GeoToScreen(airspace.GetReferenceLocation());
    unsigned screen_radius = projection.GeoToScreenDistance(airspace.GetRadius());

    if (!warning_manager.IsAcked(airspace) &&
        class_settings.fill_mode !=
        AirspaceClassRendererSettings::FillMode::NONE) {
      const GLEnable<GL_STENCIL_TEST> stencil;
      const GLEnable<GL_BLEND> blend;
      SetupInterior(airspace);
      if (warning_manager.HasWarning(airspace) ||
          warning_manager.IsInside(airspace) ||
          look.thick_pen.GetWidth() >= 2 * screen_radius ||
          class_settings.fill_mode ==
          AirspaceClassRendererSettings::FillMode::ALL) {
        // fill whole circle
        canvas.DrawCircle(screen_center, screen_radius);
      } else {
        // draw a ring inside the circle
        Color color = class_look.fill_color;
        Pen pen_donut(look.thick_pen.GetWidth() / 2, color.WithAlpha(90));
        canvas.SelectHollowBrush();
        canvas.Select(pen_donut);
        canvas.DrawCircle(screen_center,
                          screen_radius - look.thick_pen.GetWidth() / 4);
      }
    }

    // draw outline
    if (SetupOutline(airspace))
      canvas.DrawCircle(screen_center, screen_radius);
  }

  void VisitPolygon(const AirspacePolygon &airspace) {
	AirspaceClass as_type_or_class = settings.classes[airspace.GetTypeOrClass()].display ? airspace.GetTypeOrClass() : airspace.GetClass();
    if (!PreparePolygon(airspace.GetPoints()))
      return;

    const AirspaceClassRendererSettings &class_settings =
      settings.classes[as_type_or_class];

    bool fill_airspace = warning_manager.HasWarning(airspace) ||
      warning_manager.IsInside(airspace) ||
      class_settings.fill_mode ==
      AirspaceClassRendererSettings::FillMode::ALL;

    if (!warning_manager.IsAcked(airspace) &&
        class_settings.fill_mode !=
        AirspaceClassRendererSettings::FillMode::NONE) {
      const GLEnable<GL_STENCIL_TEST> stencil;

      if (!fill_airspace) {
        // set stencil for filling (bit 0)
        SetFillStencil();
        DrawPrepared();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		GLenum err5 = glGetError(); if (err5 != GL_NO_ERROR) LogFormat("AIAI OpenGL error 0x%X", err5);

      }

      // fill interior without overpainting any previous outlines
      {
        SetupInterior(airspace, !fill_airspace);
        const GLEnable<GL_BLEND> blend;
        DrawPrepared();
      }

      if (!fill_airspace) {
        // clear fill stencil (bit 0)
        ClearFillStencil();
        DrawPrepared();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		GLenum err4 = glGetError(); if (err4 != GL_NO_ERROR) LogFormat("AJAJ OpenGL error 0x%X", err4);
      }
    }

    // draw outline
    if (SetupOutline(airspace))
      DrawPrepared();
  }

public:
  void Visit(const AbstractAirspace &airspace) {
    switch (airspace.GetShape()) {
    case AbstractAirspace::Shape::CIRCLE:
      VisitCircle((const AirspaceCircle &)airspace);
      break;

    case AbstractAirspace::Shape::POLYGON:
      VisitPolygon((const AirspacePolygon &)airspace);
      break;
    }
  }

private:
  bool SetupOutline(const AbstractAirspace &airspace) {
    AirspaceClass as_type_or_class = settings.classes[airspace.GetTypeOrClass()].display ? airspace.GetTypeOrClass() : airspace.GetClass();

    if (settings.black_outline)
      canvas.SelectBlackPen();
    else if (settings.classes[as_type_or_class].border_width == 0)
      // Don't draw outlines if border_width == 0
      return false;
    else
      canvas.Select(look.classes[as_type_or_class].border_pen);

    canvas.SelectHollowBrush();

    // set bit 1 in stencil buffer, where an outline is drawn
    glStencilFunc(GL_ALWAYS, 3, 3);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("AKAK OpenGL error 0x%X", err0);
    glStencilMask(2);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("ALAL OpenGL error 0x%X", err1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("AMAM OpenGL error 0x%X", err2);

    return true;
  }

  void SetupInterior(const AbstractAirspace &airspace,
                     bool check_fillstencil = false) {
	AirspaceClass as_type_or_class = settings.classes[airspace.GetTypeOrClass()].display ? airspace.GetTypeOrClass() : airspace.GetClass();
    const AirspaceClassLook &class_look = look.classes[as_type_or_class];

    // restrict drawing area and don't paint over previously drawn outlines
    if (check_fillstencil)
      glStencilFunc(GL_EQUAL, 1, 3);
	  GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("ANAN OpenGL error 0x%X", err1);
    else
      glStencilFunc(GL_EQUAL, 0, 2);
	  GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("AOAO OpenGL error 0x%X", err2);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("APAP OpenGL error 0x%X", err0);

    canvas.Select(Brush(class_look.fill_color.WithAlpha(90)));
    canvas.SelectNullPen();
  }

  void SetFillStencil() {
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("AQAQ OpenGL error 0x%X", err3);
    glStencilFunc(GL_ALWAYS, 3, 3);
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("ARAR OpenGL error 0x%X", err2);
    glStencilMask(1);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("ASAS OpenGL error 0x%X", err1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("ATAT OpenGL error 0x%X", err0);

    canvas.SelectHollowBrush();
    canvas.Select(look.thick_pen);
  }

  void ClearFillStencil() {
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	GLenum err1 = glGetError(); if (err1 != GL_NO_ERROR) LogFormat("AUAU OpenGL error 0x%X", err1);
    glStencilFunc(GL_ALWAYS, 3, 3);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("AVAV OpenGL error 0x%X", err0);
    glStencilMask(1);
	GLenum err2 = glGetError(); if (err2 != GL_NO_ERROR) LogFormat("AWAW OpenGL error 0x%X", err2);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
	GLenum err3 = glGetError(); if (err3 != GL_NO_ERROR) LogFormat("AXAX OpenGL error 0x%X", err3);

    canvas.SelectHollowBrush();
    canvas.Select(look.thick_pen);
  }
};

class AirspaceFillRenderer final
  : protected MapCanvas
{
  const AirspaceLook &look;
  const AirspaceWarningCopy &warning_manager;
  const AirspaceRendererSettings &settings;

public:
  AirspaceFillRenderer(Canvas &_canvas, const WindowProjection &_projection,
                       const AirspaceLook &_look,
                       const AirspaceWarningCopy &_warnings,
                       const AirspaceRendererSettings &_settings)
    :MapCanvas(_canvas, _projection,
               _projection.GetScreenBounds().Scale(1.1)),
     look(_look), warning_manager(_warnings), settings(_settings)
  {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLenum err0 = glGetError(); if (err0 != GL_NO_ERROR) LogFormat("AYAY OpenGL error 0x%X", err0);
  }

private:
  void VisitCircle(const AirspaceCircle &airspace) {
    auto screen_center = projection.GeoToScreen(airspace.GetReferenceLocation());
    unsigned screen_radius = projection.GeoToScreenDistance(airspace.GetRadius());

    if (!warning_manager.IsAcked(airspace) && SetupInterior(airspace)) {
      const GLEnable<GL_BLEND> blend;
      canvas.DrawCircle(screen_center, screen_radius);
    }

    // draw outline
    if (SetupOutline(airspace))
      canvas.DrawCircle(screen_center, screen_radius);
  }

  void VisitPolygon(const AirspacePolygon &airspace) {
    if (!PreparePolygon(airspace.GetPoints()))
      return;

    if (!warning_manager.IsAcked(airspace) && SetupInterior(airspace)) {
      // fill interior without overpainting any previous outlines
      GLEnable<GL_BLEND> blend;
      DrawPrepared();
    }

    // draw outline
    if (SetupOutline(airspace))
      DrawPrepared();
  }

public:
  void Visit(const AbstractAirspace &airspace) {
    switch (airspace.GetShape()) {
    case AbstractAirspace::Shape::CIRCLE:
      VisitCircle((const AirspaceCircle &)airspace);
      break;

    case AbstractAirspace::Shape::POLYGON:
      VisitPolygon((const AirspacePolygon &)airspace);
      break;
    }
  }

private:
  bool SetupOutline(const AbstractAirspace &airspace) {
    AirspaceClass as_type_or_class = settings.classes[airspace.GetTypeOrClass()].display ? airspace.GetTypeOrClass() : airspace.GetClass();

    if (settings.black_outline)
      canvas.SelectBlackPen();
    else if (settings.classes[as_type_or_class].border_width == 0)
      // Don't draw outlines if border_width == 0
      return false;
    else
      canvas.Select(look.classes[as_type_or_class].border_pen);

    canvas.SelectHollowBrush();

    return true;
  }

  bool SetupInterior(const AbstractAirspace &airspace) {
	AirspaceClass as_type_or_class = settings.classes[airspace.GetTypeOrClass()].display ? airspace.GetTypeOrClass() : airspace.GetClass();
    if (settings.fill_mode == AirspaceRendererSettings::FillMode::NONE)
      return false;

    const AirspaceClassLook &class_look = look.classes[as_type_or_class];

    canvas.Select(Brush(class_look.fill_color.WithAlpha(48)));
    canvas.SelectNullPen();

    return true;
  }
};

void
AirspaceRenderer::DrawInternal(Canvas &canvas,
                               const WindowProjection &projection,
                               const AirspaceRendererSettings &settings,
                               const AirspaceWarningCopy &awc,
                               const AirspacePredicate &visible)
{
  const auto range =
    airspaces->QueryWithinRange(projection.GetGeoScreenCenter(),
                                projection.GetScreenDistanceMeters());

  if (settings.fill_mode == AirspaceRendererSettings::FillMode::ALL ||
      settings.fill_mode == AirspaceRendererSettings::FillMode::NONE) {
    AirspaceFillRenderer renderer(canvas, projection, look, awc, settings);
    for (const auto &i : range) {
      const AbstractAirspace &airspace = i.GetAirspace();
      if (visible(airspace))
        renderer.Visit(airspace);
    }
  } else {
    AirspaceVisitorRenderer renderer(canvas, projection, look, awc, settings);
    for (const auto &i : range) {
      const AbstractAirspace &airspace = i.GetAirspace();
      if (visible(airspace))
        renderer.Visit(airspace);
    }
  }
}

#endif /* ENABLE_OPENGL */
