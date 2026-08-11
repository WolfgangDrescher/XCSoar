// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CustomGeometryFile.hpp"
#include "Border.hpp"
#include "io/FileReader.hxx"
#include "json/Parse.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "system/Path.hpp"

#include <boost/json.hpp>

#include <cstdlib>
#include <stdexcept>

using InfoBoxLayout::CustomGeometry;

static double
GetNumber(const boost::json::value &v, const char *what)
{
  if (v.is_int64())
    return (double)v.as_int64();
  if (v.is_uint64())
    return (double)v.as_uint64();
  if (v.is_double())
    return v.as_double();

  throw FmtRuntimeError("'{}' must be a number", what);
}

static unsigned
GetPositiveInteger(const boost::json::object &o, const char *key)
{
  const double value = GetNumber(o.at(key), key);
  if (value < 1 || value > 100000 || value != (double)(unsigned)value)
    throw FmtRuntimeError("'{}' must be a positive integer", key);

  return (unsigned)value;
}

static CustomGeometry::Dimension
ParseDimension(const boost::json::value &v, const char *what)
{
  if (v.is_string()) {
    const auto &s = v.as_string();
    char *endptr;
    const double value = std::strtod(s.c_str(), &endptr);
    if (endptr == s.c_str() || *endptr != '%' || *(endptr + 1) != '\0')
      throw FmtRuntimeError("'{}': expected a number or a percentage "
                            "string like \"12.5%\"", what);

    if (value < 0 || value > 100)
      throw FmtRuntimeError("'{}': percentage out of range 0..100", what);

    return {value, true};
  }

  const double value = GetNumber(v, what);
  if (value < 0)
    throw FmtRuntimeError("'{}' must not be negative", what);

  return {value, false};
}

static CustomGeometry::Rect
ParseRect(const boost::json::object &o)
{
  return {
    ParseDimension(o.at("x"), "x"),
    ParseDimension(o.at("y"), "y"),
    ParseDimension(o.at("width"), "width"),
    ParseDimension(o.at("height"), "height"),
  };
}

static unsigned
ParseBorder(const boost::json::value &v)
{
  unsigned border = 0;

  for (const auto &i : v.as_array()) {
    const auto &s = i.as_string();
    if (s == "top")
      border |= BORDERTOP;
    else if (s == "right")
      border |= BORDERRIGHT;
    else if (s == "bottom")
      border |= BORDERBOTTOM;
    else if (s == "left")
      border |= BORDERLEFT;
    else
      throw FmtRuntimeError("unknown border edge '{}'",
                            std::string_view{s.data(), s.size()});
  }

  return border;
}

static InfoBoxFactory::Type
ParseContent(const boost::json::value &v)
{
  const double value = GetNumber(v, "content");
  if (value < 0 || value >= (double)InfoBoxFactory::NUM_TYPES ||
      value != (double)(unsigned)value)
    throw FmtRuntimeError("'content' must be an integer in range 0..{}"
                          " (an InfoBoxFactory::Type value, as stored in"
                          " the InfoBoxPanel<n>Box<i> profile keys)",
                          (unsigned)InfoBoxFactory::NUM_TYPES - 1);

  return (InfoBoxFactory::Type)(unsigned)value;
}

static CustomGeometry::Box
ParseBox(const boost::json::object &o)
{
  CustomGeometry::Box box;
  static_cast<CustomGeometry::Rect &>(box) = ParseRect(o);

  box.border = BORDERTOP | BORDERRIGHT | BORDERBOTTOM | BORDERLEFT;
  if (const auto *border = o.if_contains("border"))
    box.border = ParseBorder(*border);

  box.content = InfoBoxFactory::NUM_TYPES;
  if (const auto *content = o.if_contains("content"))
    box.content = ParseContent(*content);

  return box;
}

CustomGeometry
InfoBoxLayout::LoadCustomGeometryFile(Path path)
{
  FileReader reader{path};
  const auto root_value = Json::Parse(reader);
  const auto &root = root_value.as_object();

  CustomGeometry g;

  const auto &screen = root.at("screen").as_object();
  g.screen_width = GetPositiveInteger(screen, "width");
  g.screen_height = GetPositiveInteger(screen, "height");

  if (screen.if_contains("dpi"))
    g.screen_dpi = GetPositiveInteger(screen, "dpi");

  if (const auto *strict = root.if_contains("strict"))
    g.strict = strict->as_bool();

  if (const auto *map = root.if_contains("map"))
    g.map = ParseRect(map->as_object());

  if (const auto *vario = root.if_contains("vario"))
    g.vario = ParseRect(vario->as_object());

  const auto &boxes = root.at("boxes").as_array();
  if (boxes.empty())
    throw std::runtime_error("'boxes' must not be empty");

  if (boxes.size() > g.boxes.capacity())
    throw FmtRuntimeError("too many boxes; at most {} are supported",
                          g.boxes.capacity());

  for (const auto &i : boxes)
    g.boxes.push_back(ParseBox(i.as_object()));

  return g;
}
