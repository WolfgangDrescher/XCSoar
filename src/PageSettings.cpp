// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PageSettings.hpp"
#include "PageOverlayTitle.hpp"
#include "InfoBoxes/InfoBoxSettings.hpp"
#include "Language/Language.hpp"
#include "util/StringBuilder.hxx"
#include "util/UTF8.hpp"
#include "util/StringAPI.hxx"

#include <algorithm>
#include <cassert>

const char *
PageLayout::MakeTitle(const InfoBoxSettings &info_box_settings,
                      std::span<char> buffer,
                      const RaspStore *rasp,
                      const bool concise) const noexcept
{
  if (!valid)
    return "---";

  switch (main) {
  case PageLayout::Main::MAP:
  case PageLayout::Main::MAP_NORTH_UP:
  case PageLayout::Main::EDL_MAP:
    break;

  case PageLayout::Main::FLARM_RADAR:
    return _("FLARM Radar");

  case PageLayout::Main::THERMAL_ASSISTANT:
    return _("Thermal Assistant");

  case PageLayout::Main::HORIZON:
    return _("Horizon");

  case PageLayout::Main::MAX:
    gcc_unreachable();
  }

  assert(!buffer.empty());
  /* Callers often pass an uninitialized StaticString buffer.  Start
     with an empty C string so Overflow before the first Append does
     not return stack garbage to CalcTextSize. */
  buffer.front() = '\0';

  BasicStringBuilder<char> builder{buffer};

  try {
    if (infobox_config.enabled) {
      builder.Append(concise ? _("Info") : _("Map and InfoBoxes"));

      if (!infobox_config.auto_switch &&
          infobox_config.panel < InfoBoxSettings::MAX_PANELS) {
        builder.Append(' ');
        builder.Append(gettext(info_box_settings.panels[infobox_config.panel].name));
      }
      else {
        if (concise) {
          builder.Append(' ');
          builder.Append(C_("Status", "Auto"));
        } else {
          builder.Append(" (");
          builder.Append(C_("Status", "Auto"));
          builder.Append(')');
        }
      }
    } else {
      if (concise)
        builder.Append(_("Info Hide"));
      else
        builder.Append(_("Map (Full screen)"));
    }

    AppendOverlayTitle(builder, *this, rasp);

    switch (bottom) {
    case Bottom::NOTHING:
    case Bottom::CUSTOM:
      break;

    case Bottom::CROSS_SECTION:
      builder.Append(", XS");
      break;

    case Bottom::WEATHER_CONTROLS:
      break;

    case Bottom::MAX:
      gcc_unreachable();
    }
  } catch (BasicStringBuilder<char>::Overflow) {
    CropIncompleteUTF8(buffer.data());
  }

  return buffer.data();
}

/**
 * Append what a page always names in its short title: its main area
 * if it is no map, and a fixed InfoBox set.
 */
static void
AppendShortTitleBase(BasicStringBuilder<char> &builder, const char *start,
                     const PageLayout &layout,
                     const InfoBoxSettings &info_box_settings)
{
  switch (layout.main) {
  case PageLayout::Main::MAP:
  case PageLayout::Main::MAP_NORTH_UP:
  case PageLayout::Main::EDL_MAP:
    break;

  case PageLayout::Main::FLARM_RADAR:
    builder.Append(_("FLARM Radar"));
    break;

  case PageLayout::Main::THERMAL_ASSISTANT:
    builder.Append(_("Thermal Assistant"));
    break;

  case PageLayout::Main::HORIZON:
    builder.Append(_("Horizon"));
    break;

  case PageLayout::Main::MAX:
    gcc_unreachable();
  }

  const auto &infobox_config = layout.infobox_config;
  if (infobox_config.enabled && !infobox_config.auto_switch &&
      infobox_config.panel < InfoBoxSettings::MAX_PANELS) {
    if (*start != '\0')
      builder.Append(", ");
    const auto &panel = info_box_settings.panels[infobox_config.panel];
    builder.Append(gettext(panel.name));
  }
}

/**
 * Append what else sets a page apart, after the base of its short
 * title: the InfoBox set which is switched automatically (a page
 * without InfoBoxes is just the map), a cross section, north up.
 */
static void
AppendShortTitleExtras(BasicStringBuilder<char> &builder, const char *start,
                       const PageLayout &layout, const char *auto_panel_name)
{
  const auto append = [&builder, start](const char *text){
    if (*start != '\0')
      builder.Append(", ");
    builder.Append(text);
  };

  const auto &infobox_config = layout.infobox_config;
  if (infobox_config.enabled &&
      (infobox_config.auto_switch ||
       infobox_config.panel >= InfoBoxSettings::MAX_PANELS) &&
      *auto_panel_name != '\0')
    append(auto_panel_name);
  if (layout.bottom == PageLayout::Bottom::CROSS_SECTION)
    append(_("Cross section"));
  if (layout.main == PageLayout::Main::MAP_NORTH_UP)
    append(_("North up"));
}

const char *
PageSettings::MakeShortTitle(unsigned index,
                             const InfoBoxSettings &info_box_settings,
                             const char *auto_panel_name,
                             std::span<char> buffer) const noexcept
{
  assert(index < MAX_PAGES);
  assert(!buffer.empty());
  buffer.front() = '\0';

  BasicStringBuilder<char> builder{buffer};

  try {
    AppendShortTitleBase(builder, buffer.data(), pages[index],
                         info_box_settings);

    /* name more only if another page would get the same title */
    for (unsigned i = 0; i < n_pages; ++i) {
      if (i == index || !pages[i].IsDefined())
        continue;

      char other_buffer[64];
      other_buffer[0] = '\0';
      BasicStringBuilder<char> other{other_buffer, sizeof(other_buffer)};
      AppendShortTitleBase(other, other_buffer, pages[i], info_box_settings);
      if (StringIsEqual(other_buffer, buffer.data())) {
        AppendShortTitleExtras(builder, buffer.data(), pages[index],
                               auto_panel_name);
        break;
      }
    }
  } catch (BasicStringBuilder<char>::Overflow) {
    CropIncompleteUTF8(buffer.data());
  }

  return buffer.data();
}

void
PageSettings::SetDefaults() noexcept
{
  pages[0] = PageLayout::Default();
  pages[1] = PageLayout::FullScreen();

  std::fill(pages.begin() + 2, pages.end(), PageLayout::Undefined());

  n_pages = 2;

  distinct_zoom = true;
}

void
PageSettings::Compress() noexcept
{
  auto last = std::remove_if(pages.begin(), pages.end(),
                             [](const PageLayout &layout) {
                               return !layout.IsDefined();
                             });
  std::fill(last, pages.end(), PageLayout::Undefined());
  n_pages = std::distance(pages.begin(), last);
}
