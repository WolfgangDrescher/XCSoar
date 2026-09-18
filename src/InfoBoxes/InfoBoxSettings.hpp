// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/StaticString.hxx"
#include "util/Compiler.h"
#include "InfoBoxes/Content/Type.hpp"

#include <cstdint>

struct InfoBoxSettings {
  enum PanelIndex {
    PANEL_CIRCLING,
    PANEL_CRUISE,
    PANEL_FINAL_GLIDE,
    PANEL_AUXILIARY,
  };

  struct Panel {
    static constexpr unsigned MAX_CONTENTS = 24;

    StaticString<32u> name;
    InfoBoxFactory::Type contents[MAX_CONTENTS];

    void Clear() noexcept;

    [[gnu::pure]]
    bool IsEmpty() const noexcept;
  };

  static constexpr unsigned MAX_PANELS = 8;
  static constexpr unsigned PREASSIGNED_PANELS = 3;

  /**
   * Auto-switch to the "final glide" panel if above final glide?
   * This setting affects the #DisplayMode, and is checked by
   * GetNewDisplayMode().
   */
  bool use_final_glide;

  enum class Geometry : uint8_t {
    /** 8 infoboxes split bottom/top or left/right */
    SPLIT_8,

    /** 8 infoboxes along bottom or right */
    BOTTOM_RIGHT_8 = 1,

    /** 8 infoboxes along top or left */
    TOP_LEFT_8 = 2,

    /** @see #SPLIT_8 */
    OBSOLETE_SPLIT_8 = 3,

    /** @see #TOP_LEFT_8 */
    OBSOLETE_TOP_LEFT_8 = 4,

    /** @see #BOTTOM_RIGHT_8 */
    OBSOLETE_BOTTOM_RIGHT_8 = 5,

    /** 9 right + vario */
    RIGHT_9_VARIO = 6,

    /** infoboxes (5) along right side (square screen) */
    RIGHT_5 = 7,

    /** 12 infoboxes along bottom or right side */
    BOTTOM_RIGHT_12 = 8,

    /** 24 infoboxes along right side (3x8) */
    RIGHT_24 = 9,

    /** @see BOTTOM_RIGHT_12 */
    OBSOLETE_BOTTOM_RIGHT_12 = 10,

    /** 12 infoboxes along top or left */
    TOP_LEFT_12 = 11,

    /** 6 left, 3 right + vario */
    LEFT_6_RIGHT_3_VARIO = 12,

    /** 8 bottom + vario */
    BOTTOM_8_VARIO = 13,
    TOP_LEFT_4 = 14,
    BOTTOM_RIGHT_4 = 15,

    OBSOLETE_BOTTOM_RIGHT_4 = 16,
    OBSOLETE_TOP_LEFT_4 = 17,

    /** 8 top + vario */
    TOP_8_VARIO = 18,

    /** 16 infoboxes along right side (2x8) */
    RIGHT_16 = 19,
    LEFT_12_RIGHT_3_VARIO = 20,

    /** 10 infoboxes along top or left */
    TOP_LEFT_10 = 21,
    /** 10 infoboxes along bottom or right side */
    BOTTOM_RIGHT_10 = 22,
    /** 10 infoboxes split bottom/top or left/right */
    SPLIT_10 = 23,
    /** 12 infoboxes 3X4 split bottom/top or left/right */
    SPLIT_3X4 = 24,
    /** 15 infoboxes 3X5 split bottom/top or left/right */
    SPLIT_3X5 = 25,
    /** 18 infoboxes 3X6 split bottom/top or left/right */
    SPLIT_3X6 = 26,

  } geometry;

/*
 * scales the font for InfoBox titles and comments between 50% and 150%
 * the value of scale_title_font ranges from 50 to 150 accordingly.
 */
  unsigned scale_title_font;

  bool use_colors;

  enum class Theme : uint8_t {
    FOLLOW_GLOBAL,
    LIGHT,
    DARK,
  } theme;

  /**
   * The shape of the InfoBoxes.  Whether the map shows through them
   * is a separate choice, see #translucent.
   */
  enum class BorderStyle : uint8_t {
    BOX,
    TAB,
    SHADED,
    GLASS,

    /**
     * Rounded boxes with a small gap between them, floating over
     * the map.
     */
    FLOATING,

    /**
     * Each block of adjacent InfoBoxes forms one panel with rounded
     * corners, set back from the screen edge and from the map.
     */
    DOCK,
  } border_style;

  /**
   * What is behind the text of an InfoBox.  Only OpenGL can blend;
   * the memory canvas keeps the boxes solid.
   */
  enum class Background : uint8_t {
    SOLID,

    /**
     * The map shows through the boxes.
     */
    TRANSPARENT,

    /**
     * Translucent over a blurred copy of the map ("frosted glass"),
     * which keeps the values readable over busy terrain.
     */
    FROSTED,
  } background;

  /**
   * Does this style leave gaps between the InfoBoxes and the screen
   * edge or their neighbours?  The map shows through them.
   */
  static constexpr bool HasGaps(BorderStyle style) noexcept {
    return style == BorderStyle::FLOATING || style == BorderStyle::DOCK;
  }

  /**
   * Does the map show through the boxes?
   */
  constexpr bool IsTranslucent() const noexcept {
    return background != Background::SOLID;
  }

  constexpr bool IsFrosted() const noexcept {
    return background == Background::FROSTED;
  }

  constexpr bool HasGaps() const noexcept {
    return HasGaps(border_style);
  }

  constexpr bool IsDock() const noexcept {
    return border_style == BorderStyle::DOCK;
  }

  /**
   * Does this style draw the title on a caption bar?
   */
  constexpr bool HasCaptionBar() const noexcept {
    return border_style == BorderStyle::SHADED;
  }

  /**
   * Is the map visible through the InfoBox area, be it through
   * translucent boxes or through the gaps around them?  The map then
   * has to be drawn behind the InfoBoxes instead of ending at their
   * edge.
   */
  constexpr bool ShowsMapBehind() const noexcept {
    return IsTranslucent() || HasGaps();
  }

  Panel panels[MAX_PANELS];

  void SetDefaults() noexcept;
};
