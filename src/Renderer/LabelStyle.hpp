// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

/* Workaround: Some Win32 headers define OPAQUE as a preprocessor
 * define; see ui/canvas/memory/Color.hpp */
#ifdef OPAQUE
#undef OPAQUE
#endif

/**
 * The look of a label drawn by LabelRenderer::Draw(): the box behind
 * the text and the text itself.
 *
 * Use one of the presets as it is, or describe a style of your own;
 * a preset is not meant to be modified.
 */
struct LabelStyle {
  enum class BorderRadius : uint8_t {
    NONE,

    /** slightly rounded corners, like the waypoint labels */
    SMALL,

    /** fully rounded ends, a pill */
    FULL,
  };

  enum class Background : uint8_t {
    NONE,

    /**
     * White with #opacity, so the map shows through; opaque without
     * OpenGL.
     */
    TRANSLUCENT,

    /** white */
    OPAQUE,
  };

  /** A hairline around the box */
  enum class Border : uint8_t {
    NONE,
    BLACK,
    WHITE,
  };

  /**
   * The colour of the text, and a halo which keeps it readable
   * without a box.
   */
  enum class Text : uint8_t {
    BLACK,
    BLACK_WITH_WHITE_HALO,
    WHITE_WITH_BLACK_HALO,
  };

  /**
   * A soft shadow lifts the box off the map (see
   * BoxShadowStyle::FLOATING).  Without OpenGL, a black border
   * replaces it.
   */
  enum class Shadow : uint8_t {
    NONE,
    DEFAULT,
  };

  /** The default #opacity */
  static constexpr uint8_t DEFAULT_OPACITY = 0xa0;

  BorderRadius border_radius = BorderRadius::SMALL;
  Background background = Background::NONE;

  /** The opacity of a #Background::TRANSLUCENT box */
  uint8_t opacity = DEFAULT_OPACITY;

  Border border = Border::NONE;
  Text text = Text::BLACK;
  Shadow shadow = Shadow::NONE;

  constexpr bool HasBox() const noexcept {
    return background != Background::NONE || border != Border::NONE ||
      shadow != Shadow::NONE;
  }

  /** Black text with a white halo, without a box */
  static const LabelStyle BLACK_TEXT_WITH_HALO;

  /** White text with a black halo, without a box */
  static const LabelStyle WHITE_TEXT_WITH_HALO;

  /** A translucent white box with rounded corners and a black border */
  static const LabelStyle ROUNDED_BLACK;

  /** A translucent white box with rounded corners and a white border */
  static const LabelStyle ROUNDED_WHITE;

  /**
   * A nearly opaque pill which floats above the map, for short
   * notices
   */
  static const LabelStyle CHIP;
};

inline constexpr LabelStyle LabelStyle::BLACK_TEXT_WITH_HALO{
  .text = Text::BLACK_WITH_WHITE_HALO,
};

inline constexpr LabelStyle LabelStyle::WHITE_TEXT_WITH_HALO{
  .text = Text::WHITE_WITH_BLACK_HALO,
};

inline constexpr LabelStyle LabelStyle::ROUNDED_BLACK{
  .background = Background::TRANSLUCENT,
  .border = Border::BLACK,
};

inline constexpr LabelStyle LabelStyle::ROUNDED_WHITE{
  .background = Background::TRANSLUCENT,
  .border = Border::WHITE,
};

inline constexpr LabelStyle LabelStyle::CHIP{
  .border_radius = BorderRadius::FULL,
  .background = Background::TRANSLUCENT,
  .opacity = 0xf2,
  .shadow = Shadow::DEFAULT,
};
