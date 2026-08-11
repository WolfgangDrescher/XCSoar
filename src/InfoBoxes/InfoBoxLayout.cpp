// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/InfoBoxLayout.hpp"
#include "Border.hpp"
#include "CustomGeometry.hpp"
#include "util/Macros.hpp"

#include <algorithm> // for std::clamp()
#include <cmath> // for std::lround()
#include <optional>
#include <span>

static constexpr double CONTROLHEIGHTRATIO = 7.4;

/**
 * The number of info boxes in each geometry.
 */
static constexpr unsigned char geometry_counts[] = {
  8, 8, 8, 8, 8, 8,
  9, 5, 12, 24, 12,
  12, 9, 8, 4, 4, 4, 4,
  8, 16, 15, 10, 10, 10,
  12, // 3 rows X 4 boxes
  15, // 3 rows X 5 boxes
  18, // 3 rows X 6 boxes
  24, // CUSTOM (upper bound; the actual count comes from the JSON file)
};

namespace InfoBoxLayout {

[[gnu::const]]
static InfoBoxSettings::Geometry
ValidateGeometry(InfoBoxSettings::Geometry geometry,
                 PixelSize screen_size) noexcept;

static void
CalcInfoBoxSizes(Layout &layout, PixelSize screen_size,
                 InfoBoxSettings::Geometry geometry) noexcept;

static std::optional<Layout>
CalculateCustom(PixelRect rc, const CustomGeometry &custom) noexcept;

static Layout
CalculateFor(PixelRect rc, InfoBoxSettings::Geometry geometry,
             const std::optional<CustomGeometry> &custom) noexcept;

} // namespace InfoBoxLayout

static int
MakeTopRow(const InfoBoxLayout::Layout &layout,
           PixelRect *p, unsigned n, int left, int right, int top) noexcept
{
  PixelRect *const end = p + n;
  const int bottom = top + layout.control_size.height;
  while (p < end) {
    p->left = left;
    left += layout.control_size.width;
    p->right = left;
    p->top = top;
    p->bottom = bottom;

    ++p;
  }

  /* assign remaining pixels to last InfoBox */
  p[-1].right = right;

  return bottom;
}

static int
MakeBottomRow(const InfoBoxLayout::Layout &layout,
              PixelRect *p, unsigned n,
              int left, int right, int bottom) noexcept
{
  int top = bottom - layout.control_size.height;
  MakeTopRow(layout, p, n, left, right, top);
  return top;
}

static int
MakeLeftColumn(const InfoBoxLayout::Layout &layout,
               PixelRect *p, unsigned n,
               int left, int top, int bottom) noexcept
{
  PixelRect *const end = p + n;
  const int right = left + layout.control_size.width;
  while (p < end) {
    p->left = left;
    p->right = right;
    p->top = top;
    top += layout.control_size.height;
    p->bottom = top;

    ++p;
  }

  /* assign remaining pixels to last InfoBox */
  p[-1].bottom = bottom;

  return right;
}

static int
MakeRightColumn(const InfoBoxLayout::Layout &layout,
                PixelRect *p, unsigned n,
                int right, int top, int bottom) noexcept
{
  int left = right - layout.control_size.width;
  MakeLeftColumn(layout, p, n, left, top, bottom);
  return left;
}

InfoBoxLayout::Layout
InfoBoxLayout::Calculate(PixelRect rc, InfoBoxSettings::Geometry geometry) noexcept
{
  return CalculateFor(rc, geometry, GetCustomGeometry());
}

InfoBoxLayout::Layout
InfoBoxLayout::Calculate(PixelRect rc, InfoBoxSettings::Geometry geometry,
                         unsigned panel_index) noexcept
{
  return CalculateFor(rc, geometry, GetCustomGeometry(panel_index));
}

InfoBoxLayout::Layout
InfoBoxLayout::CalculateFor(PixelRect rc, InfoBoxSettings::Geometry geometry,
                            const std::optional<CustomGeometry> &custom) noexcept
{
  const PixelSize screen_size = rc.GetSize();

  if (geometry == InfoBoxSettings::Geometry::CUSTOM) {
    if (custom)
      if (auto custom_layout = CalculateCustom(rc, *custom))
        return *custom_layout;

    /* no valid custom layout loaded, or "strict" is set and the
       screen size does not match: fall back to a default geometry */
    geometry = InfoBoxSettings::Geometry::SPLIT_8;
  }

  geometry = ValidateGeometry(geometry, screen_size);

  Layout layout;

  layout.geometry = geometry;
  layout.landscape = screen_size.width > screen_size.height;
  layout.count = geometry_counts[(unsigned)geometry];
  assert(layout.count <= InfoBoxSettings::Panel::MAX_CONTENTS);

  CalcInfoBoxSizes(layout, screen_size, geometry);

  layout.ClearVario();

  unsigned right = rc.right;

  switch (geometry) {
  case InfoBoxSettings::Geometry::SPLIT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_SPLIT_8:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 4,
                               rc.left, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions + 4, 4,
                                 rc.right, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 4,
                          rc.left, rc.right, rc.top);
      rc.bottom = MakeBottomRow(layout, layout.positions + 4, 4,
                                rc.left, rc.right, rc.bottom);
    }

    break;

  case InfoBoxSettings::Geometry::SPLIT_10:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 5,
                               rc.left, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions + 5, 5,
                                 rc.right, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 5,
                          rc.left, rc.right, rc.top);
      rc.bottom = MakeBottomRow(layout, layout.positions + 5, 5,
                                rc.left, rc.right, rc.bottom);
    }

    break;

  case InfoBoxSettings::Geometry::BOTTOM_8_VARIO:
    layout.vario.left = rc.right - layout.control_size.width;
    layout.vario.right = rc.right;
    layout.vario.top = rc.bottom - layout.control_size.height * 2;
    layout.vario.bottom = rc.bottom;

    right = layout.vario.left;

    [[fallthrough]];

  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_8:
    if (layout.landscape) {
      rc.right = MakeRightColumn(layout, layout.positions + 4, 4,
                                 rc.right, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions, 4,
                                 rc.right, rc.top, rc.bottom);
    } else {
      rc.bottom = MakeBottomRow(layout, layout.positions + 4, 4,
                                rc.left, right, rc.bottom);
      rc.bottom = MakeBottomRow(layout, layout.positions, 4,
                                rc.left, right, rc.bottom);
    }

    break;

  case InfoBoxSettings::Geometry::TOP_8_VARIO:
    layout.vario.left = rc.right - layout.control_size.width;
    layout.vario.right = rc.right;
    layout.vario.top = rc.top;
    layout.vario.bottom = rc.top + layout.control_size.height * 2;

    right = layout.vario.left;

    [[fallthrough]];

  case InfoBoxSettings::Geometry::TOP_LEFT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_8:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 4,
                               rc.left, rc.top, rc.bottom);
      rc.left = MakeLeftColumn(layout, layout.positions + 4, 4,
                               rc.left, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 4,
                          rc.left, right, rc.top);
      rc.top = MakeTopRow(layout, layout.positions + 4, 4,
                          rc.left, right, rc.top);
    }

    break;

  case InfoBoxSettings::Geometry::LEFT_6_RIGHT_3_VARIO:
    layout.vario.left = rc.right - layout.control_size.width;
    layout.vario.right = rc.right;
    layout.vario.top = 0;
    layout.vario.bottom = layout.vario.top + layout.control_size.height * 3;

    rc.left = MakeLeftColumn(layout, layout.positions, 6,
                             rc.left, rc.top, rc.bottom);
    rc.right = MakeRightColumn(layout, layout.positions + 6, 3, rc.right,
                               rc.top + 3 * layout.control_size.height, rc.bottom);
    break;

  case InfoBoxSettings::Geometry::LEFT_12_RIGHT_3_VARIO:
    layout.vario.left = rc.right - layout.control_size.width;
    layout.vario.right = rc.right;
    layout.vario.top = 0;
    layout.vario.bottom = layout.vario.top + layout.control_size.height * 3;

    rc.right = MakeRightColumn(layout, layout.positions + 6, 3, rc.right,
                               rc.top + 3 * layout.control_size.height, rc.bottom);

    layout.control_size.width = layout.control_size.height * 1.1;
    rc.left = MakeLeftColumn(layout, layout.positions, 6,
                             rc.left, rc.top, rc.bottom);
    rc.left = MakeLeftColumn(layout, layout.positions + 9, 6,
                             rc.left, rc.top, rc.bottom);
    break;

  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_12:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_12:
    if (layout.landscape) {
      rc.right = MakeRightColumn(layout, layout.positions + 6, 6,
                                 rc.right, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions, 6,
                                 rc.right, rc.top, rc.bottom);
    } else {
      rc.bottom = MakeBottomRow(layout, layout.positions + 6, 6,
                                rc.left, rc.right, rc.bottom);
      rc.bottom = MakeBottomRow(layout, layout.positions, 6,
                                rc.left, rc.right, rc.bottom);
    }

    break;

  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_10:
    if (layout.landscape) {
      rc.right = MakeRightColumn(layout, layout.positions + 5, 5,
                                 rc.right, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions, 5,
                                 rc.right, rc.top, rc.bottom);
    } else {
      rc.bottom = MakeBottomRow(layout, layout.positions + 5, 5,
                                rc.left, rc.right, rc.bottom);
      rc.bottom = MakeBottomRow(layout, layout.positions, 5,
                                rc.left, rc.right, rc.bottom);
    }

    break;

  case InfoBoxSettings::Geometry::TOP_LEFT_10:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 5,
                               rc.left, rc.top, rc.bottom);
      rc.left = MakeLeftColumn(layout, layout.positions + 5, 5,
                               rc.left, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 5,
                          rc.left, rc.right, rc.top);
      rc.top = MakeTopRow(layout, layout.positions + 5, 5,
                          rc.left, rc.right, rc.top);
    }
    break;

  case InfoBoxSettings::Geometry::TOP_LEFT_12:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 6,
                               rc.left, rc.top, rc.bottom);
      rc.left = MakeLeftColumn(layout, layout.positions + 6, 6,
                               rc.left, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 6,
                          rc.left, rc.right, rc.top);
      rc.top = MakeTopRow(layout, layout.positions + 6, 6,
                          rc.left, rc.right, rc.top);
    }
    break;

  case InfoBoxSettings::Geometry::SPLIT_3X4:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 4,
                               rc.left, rc.top, rc.bottom);
      rc.left = MakeLeftColumn(layout, layout.positions + 4, 4,
                               rc.left, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions + 8, 4,
                               rc.right, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 4,
                          rc.left, rc.right, rc.top);
      rc.top = MakeTopRow(layout, layout.positions + 4, 4,
                          rc.left, rc.right, rc.top);
      rc.bottom = MakeBottomRow(layout, layout.positions + 8, 4,
                          rc.left, rc.right, rc.bottom);
    }
    break;

  case InfoBoxSettings::Geometry::SPLIT_3X5:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 5,
                               rc.left, rc.top, rc.bottom);
      rc.left = MakeLeftColumn(layout, layout.positions + 5, 5,
                               rc.left, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions + 10, 5,
                               rc.right, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 5,
                          rc.left, rc.right, rc.top);
      rc.top = MakeTopRow(layout, layout.positions + 5, 5,
                          rc.left, rc.right, rc.top);
      rc.bottom = MakeBottomRow(layout, layout.positions + 10, 5,
                          rc.left, rc.right, rc.bottom);
    }
    break;

  case InfoBoxSettings::Geometry::SPLIT_3X6:
    if (layout.landscape) {
      rc.left = MakeLeftColumn(layout, layout.positions, 6,
                               rc.left, rc.top, rc.bottom);
      rc.left = MakeLeftColumn(layout, layout.positions + 6, 6,
                               rc.left, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions + 12, 6,
                               rc.right, rc.top, rc.bottom);
    } else {
      rc.top = MakeTopRow(layout, layout.positions, 6,
                          rc.left, rc.right, rc.top);
      rc.top = MakeTopRow(layout, layout.positions + 6, 6,
                          rc.left, rc.right, rc.top);
      rc.bottom = MakeBottomRow(layout, layout.positions + 12, 6,
                          rc.left, rc.right, rc.bottom);
    }
    break;

  case InfoBoxSettings::Geometry::RIGHT_16:
    rc.right = MakeRightColumn(layout, layout.positions + 8, 8,
                               rc.right, rc.top, rc.bottom);
    rc.right = MakeRightColumn(layout, layout.positions, 8,
                               rc.right, rc.top, rc.bottom);
    break;

  case InfoBoxSettings::Geometry::RIGHT_24:
    if (layout.landscape) {
      rc.right = MakeRightColumn(layout, layout.positions + 16, 8,
                                 rc.right, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions + 8, 8,
                                 rc.right, rc.top, rc.bottom);
      rc.right = MakeRightColumn(layout, layout.positions, 8,
                                 rc.right, rc.top, rc.bottom);
    } else {
      rc.bottom = MakeBottomRow(layout, layout.positions + 16, 8,
                                rc.left, rc.right, rc.bottom);
      rc.bottom = MakeBottomRow(layout, layout.positions + 8, 8,
                                rc.left, rc.right, rc.bottom);
      rc.bottom = MakeBottomRow(layout, layout.positions, 8,
                                rc.left, rc.right, rc.bottom);
    }
    break;

  case InfoBoxSettings::Geometry::RIGHT_9_VARIO:
    layout.vario.left = rc.right - layout.control_size.width;
    layout.vario.right = rc.right;
    layout.vario.top = 0;
    layout.vario.bottom = layout.vario.top + layout.control_size.height * 3;

    rc.right = MakeRightColumn(layout, layout.positions + 6, 3,
                               rc.right,
                               rc.top + 3 * layout.control_size.height, rc.bottom);
    rc.right = MakeRightColumn(layout, layout.positions, 6,
                               rc.right, rc.top, rc.bottom);
    break;

  case InfoBoxSettings::Geometry::RIGHT_5:
    rc.right = MakeRightColumn(layout, layout.positions, 5,
                               rc.right, rc.top, rc.bottom);
    break;

  case InfoBoxSettings::Geometry::TOP_LEFT_4:
  case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_4:
    if (layout.landscape)
      rc.left = MakeLeftColumn(layout, layout.positions, 4,
                               rc.left, rc.top, rc.bottom);
    else
      rc.top = MakeTopRow(layout, layout.positions, 4,
                          rc.left, rc.right, rc.top);
    break;

  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_4:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_4:
    if (layout.landscape)
      rc.right = MakeRightColumn(layout, layout.positions, 4,
                                 rc.right, rc.top, rc.bottom);
    else
      rc.bottom = MakeBottomRow(layout, layout.positions, 4,
                                rc.left, rc.right, rc.bottom);
    break;

  case InfoBoxSettings::Geometry::CUSTOM:
    /* handled by CalculateCustom() above */
    gcc_unreachable();
  };

  layout.remaining = rc;
  return layout;
}

static int
ResolveDimension(InfoBoxLayout::CustomGeometry::Dimension d,
                 unsigned actual, unsigned reference) noexcept
{
  if (d.percent)
    return (int)std::lround(d.value * actual / 100.);

  /* absolute pixels in reference screen space, scaled to the actual
     screen size */
  return (int)std::lround(d.value * actual / reference);
}

static PixelRect
ResolveRect(const InfoBoxLayout::CustomGeometry::Rect &r, PixelRect rc,
            unsigned ref_width, unsigned ref_height) noexcept
{
  const PixelSize screen_size = rc.GetSize();

  const int x = rc.left + ResolveDimension(r.x, screen_size.width, ref_width);
  const int y = rc.top + ResolveDimension(r.y, screen_size.height, ref_height);
  const int width = std::max(1, ResolveDimension(r.width, screen_size.width,
                                                 ref_width));
  const int height = std::max(1, ResolveDimension(r.height, screen_size.height,
                                                  ref_height));

  PixelRect result;
  result.left = std::clamp(x, rc.left, rc.right - 1);
  result.top = std::clamp(y, rc.top, rc.bottom - 1);
  result.right = std::clamp(x + width, result.left + 1, rc.right);
  result.bottom = std::clamp(y + height, result.top + 1, rc.bottom);
  return result;
}

/**
 * Find the largest axis-aligned rectangle inside #rc which does not
 * intersect any of the #obstacles.  This becomes the map area when
 * the custom geometry does not define one explicitly.  Returns #rc
 * itself (map covering the whole screen, below the InfoBoxes) when
 * the obstacles cover everything.
 */
static PixelRect
FindLargestFreeRectangle(const PixelRect rc,
                         std::span<const PixelRect> obstacles) noexcept
{
  static constexpr std::size_t MAX_OBSTACLES =
    InfoBoxSettings::Panel::MAX_CONTENTS + 1;
  /* grid edges: two per obstacle plus the two screen edges */
  static constexpr std::size_t MAX_EDGES = 2 * (MAX_OBSTACLES + 1);
  assert(obstacles.size() <= MAX_OBSTACLES);

  /* coordinate compression: collect the distinct x/y edges */
  StaticArray<int, MAX_EDGES> xs, ys;
  xs.push_back(rc.left);
  xs.push_back(rc.right);
  ys.push_back(rc.top);
  ys.push_back(rc.bottom);

  StaticArray<PixelRect, MAX_OBSTACLES> clamped;
  for (PixelRect o : obstacles) {
    o.left = std::max(o.left, rc.left);
    o.top = std::max(o.top, rc.top);
    o.right = std::min(o.right, rc.right);
    o.bottom = std::min(o.bottom, rc.bottom);
    if (o.right <= o.left || o.bottom <= o.top)
      continue;

    clamped.push_back(o);
    xs.push_back(o.left);
    xs.push_back(o.right);
    ys.push_back(o.top);
    ys.push_back(o.bottom);
  }

  std::sort(xs.begin(), xs.end());
  std::sort(ys.begin(), ys.end());
  const unsigned nx = std::unique(xs.begin(), xs.end()) - xs.begin();
  const unsigned ny = std::unique(ys.begin(), ys.end()) - ys.begin();

  /* mark grid cells covered by an obstacle */
  bool occupied[MAX_EDGES - 1][MAX_EDGES - 1]{};
  for (const auto &o : clamped) {
    const unsigned c1 = std::lower_bound(xs.begin(), xs.begin() + nx,
                                         o.left) - xs.begin();
    const unsigned c2 = std::lower_bound(xs.begin(), xs.begin() + nx,
                                         o.right) - xs.begin();
    const unsigned r1 = std::lower_bound(ys.begin(), ys.begin() + ny,
                                         o.top) - ys.begin();
    const unsigned r2 = std::lower_bound(ys.begin(), ys.begin() + ny,
                                         o.bottom) - ys.begin();
    for (unsigned r = r1; r < r2; ++r)
      for (unsigned c = c1; c < c2; ++c)
        occupied[r][c] = true;
  }

  /* maximal-rectangle search: for each grid row, treat the free
     cells above (and including) it as a histogram and find the
     largest rectangle in it with a monotonic stack */

  /* start_row[c]: first grid row such that all rows from there down
     to the current row are free in column c */
  unsigned start_row[MAX_EDGES - 1]{};

  long best_area = 0;
  PixelRect best = rc;

  for (unsigned r = 0; r + 1 < ny; ++r) {
    for (unsigned c = 0; c + 1 < nx; ++c)
      if (occupied[r][c])
        start_row[c] = r + 1;

    const int bottom = ys[r + 1];

    struct StackItem {
      unsigned start_row;
      int left;
    } stack[MAX_EDGES];
    unsigned depth = 0;

    for (unsigned c = 0; c < nx; ++c) {
      /* the sentinel column (c == nx - 1) has zero height and
         flushes the stack */
      const unsigned sr = c + 1 < nx ? start_row[c] : r + 1;
      int left = xs[c];

      /* pop all columns taller than the current one (smaller
         start_row = taller histogram bar) */
      while (depth > 0 && stack[depth - 1].start_row < sr) {
        const StackItem item = stack[--depth];
        const int top = ys[item.start_row];
        const long area = (long)(bottom - top) * (xs[c] - item.left);
        if (area > best_area) {
          best_area = area;
          best = {item.left, top, xs[c], bottom};
        }

        left = item.left;
      }

      if (sr <= r && (depth == 0 || stack[depth - 1].start_row > sr))
        stack[depth++] = {sr, left};
    }
  }

  return best;
}

std::optional<InfoBoxLayout::Layout>
InfoBoxLayout::CalculateCustom(PixelRect rc,
                               const CustomGeometry &custom) noexcept
{
  if (custom.boxes.empty() ||
      custom.screen_width == 0 || custom.screen_height == 0)
    return std::nullopt;

  const PixelSize screen_size = rc.GetSize();

  if (custom.strict &&
      (screen_size.width != custom.screen_width ||
       screen_size.height != custom.screen_height))
    /* this layout was designed for a different screen */
    return std::nullopt;

  Layout layout;
  layout.geometry = InfoBoxSettings::Geometry::CUSTOM;
  layout.landscape = screen_size.width > screen_size.height;
  layout.count = custom.boxes.size();
  assert(layout.count <= InfoBoxSettings::Panel::MAX_CONTENTS);
  layout.ClearVario();

  unsigned min_width = screen_size.width, min_height = screen_size.height;

  StaticArray<PixelRect, InfoBoxSettings::Panel::MAX_CONTENTS + 1> obstacles;

  for (unsigned i = 0; i < layout.count; ++i) {
    const PixelRect box = ResolveRect(custom.boxes[i], rc,
                                      custom.screen_width,
                                      custom.screen_height);
    layout.positions[i] = box;
    layout.custom_borders[i] = custom.boxes[i].border;
    obstacles.push_back(box);

    const PixelSize size = box.GetSize();
    min_width = std::min(min_width, size.width);
    min_height = std::min(min_height, size.height);
  }

  /* fonts are sized for the smallest box so text fits everywhere */
  layout.control_size = {min_width, min_height};

  if (custom.vario) {
    layout.vario = ResolveRect(*custom.vario, rc,
                               custom.screen_width, custom.screen_height);
    obstacles.push_back(layout.vario);
  }

  if (custom.map)
    layout.remaining = ResolveRect(*custom.map, rc,
                                   custom.screen_width,
                                   custom.screen_height);
  else
    layout.remaining = FindLargestFreeRectangle(rc, obstacles);

  return layout;
}

static InfoBoxSettings::Geometry
InfoBoxLayout::ValidateGeometry(InfoBoxSettings::Geometry geometry,
                                PixelSize screen_size) noexcept
{
  if ((unsigned)geometry >= ARRAY_SIZE(geometry_counts))
    /* out of range */
    geometry = InfoBoxSettings::Geometry::SPLIT_8;

  if (screen_size.width > screen_size.height) {
    /* landscape */

    switch (geometry) {
    case InfoBoxSettings::Geometry::SPLIT_8:
    case InfoBoxSettings::Geometry::SPLIT_10:
    case InfoBoxSettings::Geometry::SPLIT_3X4:
    case InfoBoxSettings::Geometry::SPLIT_3X5:
    case InfoBoxSettings::Geometry::SPLIT_3X6:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_8:
    case InfoBoxSettings::Geometry::TOP_LEFT_8:
    case InfoBoxSettings::Geometry::OBSOLETE_SPLIT_8:
    case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_8:
    case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_8:
    case InfoBoxSettings::Geometry::RIGHT_9_VARIO:
    case InfoBoxSettings::Geometry::RIGHT_5:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_12:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_10:
    case InfoBoxSettings::Geometry::RIGHT_16:
    case InfoBoxSettings::Geometry::RIGHT_24:
    case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_12:
    case InfoBoxSettings::Geometry::TOP_LEFT_12:
    case InfoBoxSettings::Geometry::TOP_LEFT_10:
    case InfoBoxSettings::Geometry::LEFT_6_RIGHT_3_VARIO:
    case InfoBoxSettings::Geometry::LEFT_12_RIGHT_3_VARIO:
    case InfoBoxSettings::Geometry::CUSTOM:
      break;

    case InfoBoxSettings::Geometry::BOTTOM_8_VARIO:
      return InfoBoxSettings::Geometry::RIGHT_9_VARIO;

    case InfoBoxSettings::Geometry::TOP_LEFT_4:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_4:
    case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_4:
    case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_4:
      break;

    case InfoBoxSettings::Geometry::TOP_8_VARIO:
      return InfoBoxSettings::Geometry::LEFT_6_RIGHT_3_VARIO;
    }
  } else {
    /* portrait and square */

    switch (geometry) {
    case InfoBoxSettings::Geometry::SPLIT_8:
    case InfoBoxSettings::Geometry::SPLIT_10:
    case InfoBoxSettings::Geometry::SPLIT_3X4:
    case InfoBoxSettings::Geometry::SPLIT_3X5:
    case InfoBoxSettings::Geometry::SPLIT_3X6:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_8:
    case InfoBoxSettings::Geometry::TOP_LEFT_8:
    case InfoBoxSettings::Geometry::OBSOLETE_SPLIT_8:
    case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_8:
    case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_8:
    case InfoBoxSettings::Geometry::RIGHT_24:
      break;

    case InfoBoxSettings::Geometry::RIGHT_9_VARIO:
      return InfoBoxSettings::Geometry::BOTTOM_8_VARIO;

    case InfoBoxSettings::Geometry::RIGHT_5:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_12:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_10:
      break;

    case InfoBoxSettings::Geometry::RIGHT_16:
      return InfoBoxSettings::Geometry::BOTTOM_RIGHT_12;

    case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_12:
    case InfoBoxSettings::Geometry::TOP_LEFT_12:
    case InfoBoxSettings::Geometry::TOP_LEFT_10:
      break;

    case InfoBoxSettings::Geometry::LEFT_6_RIGHT_3_VARIO:
      return InfoBoxSettings::Geometry::BOTTOM_8_VARIO;

    case InfoBoxSettings::Geometry::LEFT_12_RIGHT_3_VARIO:
      return InfoBoxSettings::Geometry::BOTTOM_8_VARIO;

    case InfoBoxSettings::Geometry::BOTTOM_8_VARIO:
    case InfoBoxSettings::Geometry::TOP_LEFT_4:
    case InfoBoxSettings::Geometry::BOTTOM_RIGHT_4:
    case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_4:
    case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_4:
    case InfoBoxSettings::Geometry::TOP_8_VARIO:
    case InfoBoxSettings::Geometry::CUSTOM:
      break;
    }
  }

  return geometry;
}

static constexpr unsigned
CalculateInfoBoxRowHeight(unsigned screen_height,
                          unsigned control_width) noexcept
{
  return std::clamp(unsigned(screen_height / CONTROLHEIGHTRATIO),
                    control_width * 5 / 7,
                    control_width);
}

static constexpr unsigned
CalculateInfoBoxColumnWidth(unsigned screen_width,
                            unsigned control_height) noexcept
{
  return std::clamp(unsigned(screen_width / CONTROLHEIGHTRATIO * 1.3),
                    control_height,
                    control_height * 7 / 5);
}

void
InfoBoxLayout::CalcInfoBoxSizes(Layout &layout, PixelSize screen_size,
                                InfoBoxSettings::Geometry geometry) noexcept
{
  const bool landscape = screen_size.width > screen_size.height;

  switch (geometry) {
  case InfoBoxSettings::Geometry::SPLIT_8:
  case InfoBoxSettings::Geometry::SPLIT_10:
  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_8:
  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_12:
  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_10:
  case InfoBoxSettings::Geometry::TOP_LEFT_8:
  case InfoBoxSettings::Geometry::TOP_LEFT_12:
  case InfoBoxSettings::Geometry::TOP_LEFT_10:
    if (landscape) {
      layout.control_size.height = 2 * screen_size.height / layout.count;
      layout.control_size.width = CalculateInfoBoxColumnWidth(screen_size.width,
                                                              layout.control_size.height);
    } else {
      layout.control_size.width = 2 * screen_size.width / layout.count;
      layout.control_size.height = CalculateInfoBoxRowHeight(screen_size.height,
                                                             layout.control_size.width);
    }

    break;

  case InfoBoxSettings::Geometry::SPLIT_3X4:
  case InfoBoxSettings::Geometry::SPLIT_3X5:
  case InfoBoxSettings::Geometry::SPLIT_3X6:
     if (landscape) {
      layout.control_size.height = 3 * screen_size.height / layout.count;
      layout.control_size.width = CalculateInfoBoxColumnWidth(screen_size.width,
                                                              layout.control_size.height);
    } else {
      layout.control_size.width = 3 * screen_size.width / layout.count;
      layout.control_size.height = CalculateInfoBoxRowHeight(screen_size.height,
                                                             layout.control_size.width);
    }

    break;

  case InfoBoxSettings::Geometry::TOP_LEFT_4:
  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_4:
    if (landscape) {
      layout.control_size.height = screen_size.height / layout.count;
      layout.control_size.width = CalculateInfoBoxColumnWidth(screen_size.width,
                                                              layout.control_size.height);
    } else {
      layout.control_size.width = screen_size.width / layout.count;
      layout.control_size.height = CalculateInfoBoxRowHeight(screen_size.height,
                                                             layout.control_size.width);
    }

    break;

  case InfoBoxSettings::Geometry::BOTTOM_8_VARIO:
    // calculate control dimensions
    layout.control_size.width = 2 * screen_size.width / (layout.count + 2);
    layout.control_size.height = CalculateInfoBoxRowHeight(screen_size.height,
                                                           layout.control_size.width);
    break;

  case InfoBoxSettings::Geometry::TOP_8_VARIO:
    // calculate control dimensions
    layout.control_size.width = 2 * screen_size.width / (layout.count + 2);
    layout.control_size.height = CalculateInfoBoxRowHeight(screen_size.height,
                                                           layout.control_size.width);
    break;

  case InfoBoxSettings::Geometry::RIGHT_9_VARIO:
  case InfoBoxSettings::Geometry::LEFT_6_RIGHT_3_VARIO:
    // calculate control dimensions
    layout.control_size.height = screen_size.height / 6;
    // preserve relative shape
    layout.control_size.width = layout.control_size.height * 1.44;
    break;

  case InfoBoxSettings::Geometry::LEFT_12_RIGHT_3_VARIO:
    // calculate control dimensions
    layout.control_size.height = screen_size.height / 6;
    // preserve relative shape
    layout.control_size.width = layout.control_size.height * 1.35;
    break;

  case InfoBoxSettings::Geometry::RIGHT_5:
    // calculate control dimensions
    layout.control_size.width = screen_size.width / 5;
    layout.control_size.height = screen_size.height / 5;
    break;

  case InfoBoxSettings::Geometry::RIGHT_16:
    layout.control_size.height = screen_size.height / 8;
    layout.control_size.width = layout.control_size.height * 1.44;
    break;

  case InfoBoxSettings::Geometry::RIGHT_24:
    if (landscape) {
      layout.control_size.height = screen_size.height / 8;
      layout.control_size.width = layout.control_size.height * 1.44;
    } else {
      layout.control_size.width = 3 * screen_size.width / layout.count;
      layout.control_size.height = CalculateInfoBoxRowHeight(screen_size.height,
                                                             layout.control_size.width);
    }
    break;

  case InfoBoxSettings::Geometry::OBSOLETE_SPLIT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_4:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_4:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_12:
  case InfoBoxSettings::Geometry::CUSTOM:
    /* CalculateCustom() bypasses this function */
    gcc_unreachable();
  }
}

int
InfoBoxLayout::GetBorder(InfoBoxSettings::Geometry geometry, bool landscape,
                         unsigned i) noexcept
{
  unsigned border = 0;

  switch (geometry) {
  case InfoBoxSettings::Geometry::SPLIT_8:
    if (landscape) {
      if (i != 3 && i != 7)
        border |= BORDERBOTTOM;

      if (i < 4)
        border |= BORDERRIGHT;
      else
        border |= BORDERLEFT;
    } else {
      if (i < 4)
        border |= BORDERBOTTOM;
      else
        border |= BORDERTOP;

      if (i != 3 && i != 7)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::SPLIT_3X4:
    if (landscape) {
      if (i != 3 && i != 7 && i != 11)
        border |= BORDERBOTTOM;

      if (i < 8)
        border |= BORDERRIGHT;
      else
        border |= BORDERLEFT;
    } else {
      if (i < 8)
        border |= BORDERBOTTOM;
      else
        border |= BORDERTOP;

      if (i != 3 && i != 7 && i != 11)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::SPLIT_3X5:
    if (landscape) {
      if (i != 4 && i != 9 && i != 14)
        border |= BORDERBOTTOM;

      if (i < 10)
        border |= BORDERRIGHT;
      else
        border |= BORDERLEFT;
    } else {
      if (i < 10)
        border |= BORDERBOTTOM;
      else
        border |= BORDERTOP;

      if (i != 4 && i != 9 && i != 14)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::SPLIT_3X6:
    if (landscape) {
      if (i != 5 && i != 11 && i != 17)
        border |= BORDERBOTTOM;

      if (i < 12)
        border |= BORDERRIGHT;
      else
        border |= BORDERLEFT;
    } else {
      if (i < 12)
        border |= BORDERBOTTOM;
      else
        border |= BORDERTOP;

      if (i != 5 && i != 11 && i != 17)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::SPLIT_10:
    if (landscape) {
      if (i != 4 && i != 9)
        border |= BORDERBOTTOM;

      if (i < 5)
        border |= BORDERRIGHT;
      else
        border |= BORDERLEFT;
    } else {
      if (i < 5)
        border |= BORDERBOTTOM;
      else
        border |= BORDERTOP;

      if (i != 4 && i != 9)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_4:
  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_8:
    if (landscape) {
      if (i != 3 && i != 7)
        border |= BORDERBOTTOM;

      border |= BORDERLEFT;
    } else {
      border |= BORDERTOP;

      if (i != 3 && i != 7)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_10:
    if (landscape) {
      if (i % 5 != 0)
        border |= BORDERTOP;
      border |= BORDERLEFT;
    } else {
      border |= BORDERTOP;

      if (i != 4 && i != 9)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::BOTTOM_RIGHT_12:
    if (landscape) {
      if (i % 6 != 0)
        border |= BORDERTOP;
      border |= BORDERLEFT;
    } else {
      border |= BORDERTOP;

      if (i != 5 && i != 11)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::TOP_LEFT_10:
    if (landscape) {
      if (i % 5 != 0)
        border |= BORDERTOP;
      border |= BORDERRIGHT;
    } else {
      border |= BORDERBOTTOM;

      if (i != 4 && i != 9)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::TOP_LEFT_12:
    if (landscape) {
      if (i % 6 != 0)
        border |= BORDERTOP;
      border |= BORDERRIGHT;
    } else {
      border |= BORDERBOTTOM;

      if (i != 5 && i != 11)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::TOP_LEFT_8:
  case InfoBoxSettings::Geometry::TOP_LEFT_4:
    if (landscape) {
      if (i != 3 && i != 7)
        border |= BORDERBOTTOM;

      border |= BORDERRIGHT;
    } else {
      border |= BORDERBOTTOM;

      if (i != 3 && i != 7)
        border |= BORDERRIGHT;
    }

    break;

  case InfoBoxSettings::Geometry::BOTTOM_8_VARIO:
    border |= BORDERTOP|BORDERRIGHT;
    break;

  case InfoBoxSettings::Geometry::TOP_8_VARIO:
    border |= BORDERBOTTOM|BORDERRIGHT;
    break;

  case InfoBoxSettings::Geometry::LEFT_6_RIGHT_3_VARIO:
    if (i != 0)
      border |= BORDERTOP;
    if (i < 6)
      border |= BORDERRIGHT;
    else
      border |= BORDERLEFT;
    break;

  case InfoBoxSettings::Geometry::LEFT_12_RIGHT_3_VARIO:
    if (!((i == 0) ||(i == 9)))
      border |= BORDERTOP;
    if (i < 12)
      border |= BORDERRIGHT;
    else
      border |= BORDERLEFT;
    break;

  case InfoBoxSettings::Geometry::RIGHT_9_VARIO:
    if (i != 0)
      border |= BORDERTOP;
    if (i < 6)
      border |= BORDERLEFT|BORDERRIGHT;
    break;

  case InfoBoxSettings::Geometry::RIGHT_5:
    border |= BORDERLEFT;
    if (i != 0)
      border |= BORDERTOP;
    break;

  case InfoBoxSettings::Geometry::RIGHT_16:
    if (i % 8 != 0)
      border |= BORDERTOP;
    border |= BORDERLEFT;
    break;

  case InfoBoxSettings::Geometry::RIGHT_24:
    if (landscape) {
      if (i % 8 != 0)
        border |= BORDERTOP;
      border |= BORDERLEFT;
    } else {
      border |= BORDERTOP;

      if (i != 7 && i != 15 && i != 23)
        border |= BORDERRIGHT;
    }
    break;

  case InfoBoxSettings::Geometry::CUSTOM:
    /* not applicable; custom borders are in Layout::custom_borders */
    break;

  case InfoBoxSettings::Geometry::OBSOLETE_SPLIT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_TOP_LEFT_4:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_8:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_4:
  case InfoBoxSettings::Geometry::OBSOLETE_BOTTOM_RIGHT_12:
    gcc_unreachable();
  }

  return border;
}
