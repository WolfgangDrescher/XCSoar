// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"
#include "Math/FastRotation.hpp"
#include "Math/Point2D.hpp"
#include "Math/Util.hpp"
#include "ui/dim/Point.hpp"

#include <cassert>

/**
 * This is a class that can be used for converting geographical into screen
 * coordinates and vice-versa.
 *
 * For doing so one needs to at least set a scaling factor (m/px) by calling
 * the SetScale() function.
 *
 *  Optional features
 * -------------------
 *
 * ScreenOrigin: By calling SetScreenOrigin the screen origin offset
 * can be set. This is the offset that the screen coordinates will be shifted
 * in the conversion functions. It is also the point of rotation for the
 * ScreenRotation.
 *
 * GeoLocation: By calling the SetGeoLocation() function the ScreenOrigin can
 * be mapped to a new geographical location, which will be taken into account
 * when converting other coordinates.
 *
 * ScreenRotation: By calling SetScreenAngle() the rotation angle for the
 * conversions can be set.
 *
 * ScreenTilt: By calling SetScreenTilt() the map plane is tilted away
 * from the viewer around the horizontal screen axis through the
 * ScreenOrigin, resulting in a perspective ("bird's eye") view: the
 * area above the ScreenOrigin appears compressed and farther away.
 */
class Projection
{
  /** This is the geographical location that the ScreenOrigin is mapped to */
  GeoPoint geo_location = GeoPoint::Invalid();

  /**
   * This is the point that the ScreenRotation will rotate around.
   * It is also the point that the GeoLocation points to.
   */
  PixelPoint screen_origin = {0, 0};

  Angle screen_angle = Angle::Zero();

  /**
   * FastIntegerRotation instance for fast
   * rotation in the conversion functions
   */
  FastIntegerRotation screen_rotation;

  /** The tilt angle of the map plane (zero = plain top-down view) */
  Angle screen_tilt = Angle::Zero();

  /** Cosine of the tilt angle (vertical compression factor) */
  double tilt_cos = 1;

  /**
   * Perspective foreshortening factor [1/px]: sin(tilt) divided by
   * the virtual camera distance.  Zero disables the tilt entirely.
   */
  double tilt_foreshortening = 0;

  /** The earth's radius in screen coordinates (px) */
  double draw_scale;
  /** Inverted value of DrawScale for faster calculations */
  double inv_draw_scale;

  /** This is the scaling factor in px/m */
  double scale;

public:
  Projection() noexcept;

  bool IsValid() const noexcept {
    return geo_location.IsValid();
  }

  [[gnu::pure]]
  double GetScale() const noexcept {
    return scale;
  }

  /**
   * Sets the scaling factor
   * @param _scale New scale in px/m
   */
  void SetScale(double _scale) noexcept;

  /**
   * Convert a pixel distance to a physical length in meters.
   */
  [[gnu::pure]]
  double DistancePixelsToMeters(const int x) const noexcept {
    return double(x) / GetScale();
  }

  [[gnu::pure]]
  double DistanceMetersToPixels(const double distance) const noexcept {
    return distance * GetScale();
  }

  /**
   * Convert a pixel distance to an angle on Earth's surface.
   */
  [[gnu::pure]]
  Angle PixelsToAngle(int pixels) const noexcept {
    return Angle::Radians(pixels * inv_draw_scale);
  }

  /**
   * Convert a an angle on Earth's surface to a pixel distance.
   */
  [[gnu::pure]]
  double AngleToPixels(Angle angle) const noexcept {
    return angle.Radians() * draw_scale;
  }

  /**
   * Converts screen coordinates to a GeoPoint
   */
  [[gnu::pure]]
  GeoPoint ScreenToGeo(PixelPoint p) const noexcept;

  /**
   * Converts a GeoPoint to screen coordinates
   * @param g GeoPoint to convert
   */
  [[gnu::pure]]
  PixelPoint GeoToScreen(const GeoPoint &g) const noexcept;

  /**
   * Returns the origin/rotation center in screen coordinates
   * @return The origin/rotation center in screen coordinates
   */
  const PixelPoint &GetScreenOrigin() const noexcept {
    return screen_origin;
  }

  /**
   * Set the origin/rotation center to the given screen coordinates
   * @param x Screen coordinate in x-direction
   * @param y Screen coordinate in y-direction
   */
  void SetScreenOrigin(int x, int y) noexcept {
    screen_origin.x = x;
    screen_origin.y = y;
  }

  /**
   * Set the origin/rotation center to the given screen coordinates
   * @param pt Screen coordinate
   */
  void SetScreenOrigin(PixelPoint pt) noexcept {
    screen_origin = pt;
  }

  /**
   * Returns the GeoPoint at the ScreenOrigin
   * @return GeoPoint at the ScreenOrigin
   */
  const GeoPoint &GetGeoLocation() const noexcept {
    assert(IsValid());

    return geo_location;
  }

  /**
   * Set the GeoPoint that relates to the ScreenOrigin
   * @param g The new GeoPoint
   */
  void SetGeoLocation(GeoPoint g) noexcept {
    geo_location = g;
    geo_location.Normalize();
  }

  /**
   * Converts a geographical distance (m) to a screen distance (px)
   * @param x A geographical distance (m)
   * @return The converted distance in px
   */
  unsigned GeoToScreenDistance(const double x) const noexcept {
    return uround(scale * x);
  }

  /**
   * Returns the current screen rotation angle
   * @return Screen rotation angle
   */
  Angle GetScreenAngle() const noexcept {
    return screen_angle;
  }

  /**
   * Sets the screen rotation angle
   * @param angle New screen rotation angle
   */
  void SetScreenAngle(Angle angle) noexcept {
    screen_rotation = screen_angle = angle;
  }

  /**
   * Creates a FastRowRotation object base on the current screen
   * rotation angle and the specified screen row.
   */
  FastRowRotation GetScreenAngleRotation(int y) const noexcept {
    return FastRowRotation(screen_rotation, y);
  }

  /**
   * Is a perspective tilt currently applied?
   */
  bool HasScreenTilt() const noexcept {
    return tilt_foreshortening > 0;
  }

  /**
   * Returns the current tilt angle of the map plane
   */
  Angle GetScreenTilt() const noexcept {
    return screen_tilt;
  }

  double GetTiltCos() const noexcept {
    return tilt_cos;
  }

  double GetTiltForeshortening() const noexcept {
    return tilt_foreshortening;
  }

  /**
   * Tilts the map plane around the horizontal screen axis through the
   * ScreenOrigin.
   *
   * @param tilt the tilt angle (zero disables the tilt)
   * @param perspective_distance the virtual camera distance [px];
   * must be large enough to keep the horizon above the top edge of
   * the screen
   */
  void SetScreenTilt(Angle tilt, double perspective_distance) noexcept;

protected:
  /**
   * Applies the perspective tilt to an untilted screen offset
   * (relative to the ScreenOrigin, y pointing down).
   */
  [[gnu::pure]]
  DoublePoint2D ApplyScreenTilt(DoublePoint2D v) const noexcept;

  /**
   * Reverses the perspective tilt; inverse of ApplyScreenTilt().
   */
  [[gnu::pure]]
  DoublePoint2D InverseScreenTilt(DoublePoint2D v) const noexcept;
};
