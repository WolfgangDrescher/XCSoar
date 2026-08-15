// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ButtonLook.hpp"
#include "Colors.hpp"
#include "Asset.hpp"

void
ButtonLook::Initialise(const Font &_font, bool dark_mode)
{
  font = &_font;

  if (dark_mode) {
    standard.foreground_color = COLOR_WHITE;
    standard.foreground_brush.Create(standard.foreground_color);
    standard.background_color = COLOR_DARK_THEME_BUTTON;
    /* pressed goes down the scale, like a Tailwind
       `active:bg-slate-800` below a slate-700 face */
    standard.pressed_background_color =
      MixColors(COLOR_BLACK, standard.background_color, 0x50);
    standard.ring_color =
      MixColors(COLOR_WHITE, standard.background_color, 0x30);

    /* "soft" primary tint on the dark background */
    selected.foreground_color = COLOR_WHITE;
    selected.foreground_brush.Create(selected.foreground_color);
    selected.background_color =
      MixColors(COLOR_XCSOAR, COLOR_DARK_THEME_BACKGROUND, 0x99);
    selected.pressed_background_color =
      MixColors(COLOR_XCSOAR, COLOR_DARK_THEME_BACKGROUND, 0x4d);
    selected.ring_color = selected.background_color;

    focused.foreground_color = COLOR_WHITE;
    focused.foreground_brush.Create(focused.foreground_color);
    focused.background_color = COLOR_XCSOAR;
    focused.pressed_background_color = COLOR_XCSOAR_PRESSED;
    /* solid faces have no border ring in Nuxt UI */
    focused.ring_color = focused.background_color;

    /* the palette's light primary, like Tailwind's
       ring-primary-300: lighter than face and page, but saturated
       enough not to read as a glow */
    focus_ring_color = COLOR_XCSOAR_LIGHT;
    selected_ring_color = COLOR_XCSOAR_PRESSED;

    disabled.color = COLOR_GRAY;
    disabled.brush.Create(disabled.color);
  } else {
    standard.foreground_color = COLOR_BLACK;
    standard.foreground_brush.Create(standard.foreground_color);
    standard.background_color = IsDithered() ? COLOR_WHITE : COLOR_BUTTON_FACE;
    standard.pressed_background_color = IsDithered() || !HasColors()
      ? COLOR_VERY_LIGHT_GRAY
      : COLOR_BUTTON_PRESSED;
    if (IsDithered() || !HasColors())
      standard.ring_color = COLOR_BLACK;
    else
      standard.ring_color = COLOR_BUTTON_RING;

    /* "soft" primary tint: light primary wash with primary text */
    if (IsDithered()) {
      selected.foreground_color = COLOR_BLACK;
      selected.background_color = COLOR_VERY_LIGHT_GRAY;
      selected.ring_color = COLOR_BLACK;
    } else if (!HasColors()) {
      selected.foreground_color = COLOR_BLACK;
      selected.background_color = COLOR_VERY_LIGHT_GRAY;
      selected.ring_color = COLOR_BLACK;
    } else {
      selected.foreground_color = COLOR_XCSOAR_DARK;
      /* one shade deeper than a classic primary-50 wash, so the
         tint remains visible on the slate page background */
      selected.background_color = MixColors(COLOR_XCSOAR, COLOR_WHITE, 0x40);
    }
    selected.foreground_brush.Create(selected.foreground_color);
    if (IsDithered() || !HasColors())
      selected.pressed_background_color = COLOR_VERY_LIGHT_GRAY;
    else {
      /* several steps down the primary scale */
      selected.pressed_background_color =
        MixColors(COLOR_XCSOAR, COLOR_WHITE, 0x99);
    }

    focused.foreground_color = COLOR_WHITE;
    focused.foreground_brush.Create(focused.foreground_color);
    focused.background_color = IsDithered() ? COLOR_BLACK : COLOR_XCSOAR;
    focused.pressed_background_color = IsDithered() || !HasColors()
      ? COLOR_BLACK
      : COLOR_XCSOAR_PRESSED;
    /* solid faces have no border ring in Nuxt UI */
    focused.ring_color = focused.background_color;

    /* the palette's light primary, like Tailwind's
       ring-primary-300: lighter than the face, but fully saturated
       instead of a white-washed pastel, which read as a pale glow */
    focus_ring_color = IsDithered() || !HasColors()
      ? COLOR_BLACK
      : COLOR_XCSOAR_LIGHT;
    selected_ring_color = IsDithered() || !HasColors()
      ? COLOR_BLACK
      : COLOR_XCSOAR_PRESSED;

    disabled.color = COLOR_GRAY;
    disabled.brush.Create(disabled.color);
  }
}
