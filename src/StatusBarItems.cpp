// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "StatusBarItems.hpp"
#include "util/StringBuilder.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/NumberParser.hpp"

#include <algorithm>
#include <cassert>

using Symbol = StatusBarItem::Symbol;
using Zone = StatusBarItem::Zone;

/**
 * The names of the symbols in the profile.
 */
static constexpr const char *symbol_names[] = {
  "battery",
  "gps",
  "flarm",
  "ballast",
  "logger",
  "network",
  "page",
  "title",
  "source",
};

static_assert(std::size(symbol_names) == unsigned(Symbol::COUNT));

/**
 * The names of the zones in the profile.
 */
static constexpr const char *zone_names[] = {
  "left",
  "right",
};

static_assert(std::size(zone_names) == unsigned(Zone::COUNT));

static constexpr char INFOBOX_PREFIX[] = "infobox:";

void
StatusBarItems::SetDefaults() noexcept
{
  Clear();
  Append(StatusBarItem::Make(Zone::LEFT, Symbol::DATA_SOURCE));
  Append(StatusBarItem::Make(Zone::LEFT, Symbol::PAGE_NUMBER));
  Append(StatusBarItem::Make(Zone::LEFT, Symbol::PAGE_TITLE));
  Append(StatusBarItem::Make(Zone::RIGHT, InfoBoxFactory::e_TimeLocal));
  Append(StatusBarItem::Make(Zone::RIGHT, Symbol::LOGGER));
  Append(StatusBarItem::Make(Zone::RIGHT, Symbol::BALLAST));
  Append(StatusBarItem::Make(Zone::RIGHT, Symbol::FLARM));
  Append(StatusBarItem::Make(Zone::RIGHT, Symbol::GPS));
  Append(StatusBarItem::Make(Zone::RIGHT, Symbol::NETWORK));
  Append(StatusBarItem::Make(Zone::RIGHT, Symbol::BATTERY));
}

void
StatusBarItems::Insert(unsigned i, StatusBarItem item) noexcept
{
  assert(i <= n_items);

  if (IsFull())
    return;

  std::copy_backward(items.begin() + i, items.begin() + n_items,
                     items.begin() + n_items + 1);
  items[i] = item;
  ++n_items;
}

void
StatusBarItems::Append(StatusBarItem item) noexcept
{
  Insert(GetZoneBegin(Zone(unsigned(item.zone) + 1)), item);
}

void
StatusBarItems::Remove(unsigned i) noexcept
{
  assert(i < n_items);

  std::copy(items.begin() + i + 1, items.begin() + n_items,
            items.begin() + i);
  --n_items;
}

unsigned
StatusBarItems::GetZoneBegin(Zone zone) const noexcept
{
  unsigned i = 0;
  while (i < n_items && items[i].zone < zone)
    ++i;
  return i;
}

std::span<const StatusBarItem>
StatusBarItems::GetZone(Zone zone) const noexcept
{
  const unsigned begin = GetZoneBegin(zone);
  const unsigned end = GetZoneBegin(Zone(unsigned(zone) + 1));
  return {items.data() + begin, end - begin};
}

/**
 * Parse one item of the list.
 *
 * @return false if the item is unknown
 */
static bool
ParseItem(const char *s, Zone zone, StatusBarItem &item) noexcept
{
  if (const char *number = StringAfterPrefix(s, INFOBOX_PREFIX)) {
    char *end;
    const unsigned type = ParseUnsigned(number, &end);
    if (end == number || *end != '\0' || type >= InfoBoxFactory::NUM_TYPES)
      return false;

    item = StatusBarItem::Make(zone, InfoBoxFactory::Type(type));
    return true;
  }

  for (unsigned i = 0; i < std::size(symbol_names); ++i) {
    if (StringIsEqual(s, symbol_names[i])) {
      item = StatusBarItem::Make(zone, Symbol(i));
      return true;
    }
  }

  return false;
}

/**
 * Copy the part of the string up to the given separator (or its end)
 * to the buffer.
 *
 * @return the rest after the separator, or nullptr if the part does
 * not fit
 */
static const char *
NextToken(const char *s, char separator, std::span<char> buffer) noexcept
{
  const char *end = StringFind(s, separator);
  if (end == nullptr)
    end = s + StringLength(s);

  const std::size_t length = end - s;
  if (length >= buffer.size())
    return nullptr;

  std::copy(s, end, buffer.data());
  buffer[length] = '\0';
  return *end == separator ? end + 1 : end;
}

bool
StatusBarItems::Parse(const char *s) noexcept
{
  Clear();
  bool found = false;

  /* zones separated by ';', each "name=item,item" */
  while (*s != '\0') {
    char section[1024];
    const char *next = NextToken(s, ';', std::span{section});
    if (next == nullptr)
      break;
    s = next;

    const char *equals = StringFind(section, '=');
    if (equals == nullptr)
      continue;

    const std::string_view name{section, std::size_t(equals - section)};
    unsigned z = 0;
    while (z < std::size(zone_names) && name != zone_names[z])
      ++z;
    if (z >= std::size(zone_names))
      /* a zone this version does not know */
      continue;

    found = true;

    const char *p = equals + 1;
    while (*p != '\0' && !IsFull()) {
      char token[32];
      const char *rest = NextToken(p, ',', std::span{token});
      if (rest == nullptr) {
        /* skip an overlong item */
        const char *comma = StringFind(p, ',');
        p = comma != nullptr ? comma + 1 : p + StringLength(p);
        continue;
      }
      p = rest;

      StatusBarItem item;
      if (ParseItem(token, Zone(z), item))
        Append(item);
    }
  }

  return found;
}

void
StatusBarItems::Format(std::span<char> buffer) const noexcept
{
  assert(!buffer.empty());
  buffer.front() = '\0';

  BasicStringBuilder<char> builder{buffer};

  try {
    for (unsigned z = 0; z < unsigned(Zone::COUNT); ++z) {
      if (z > 0)
        builder.Append(';');
      builder.Append(zone_names[z]);
      builder.Append('=');

      bool first = true;
      for (const auto &item : GetZone(Zone(z))) {
        if (!first)
          builder.Append(',');
        first = false;

        switch (item.kind) {
        case StatusBarItem::Kind::SYMBOL:
          builder.Append(symbol_names[item.id]);
          break;

        case StatusBarItem::Kind::INFOBOX:
          builder.Append(INFOBOX_PREFIX);
          builder.Format("%u", unsigned(item.id));
          break;
        }
      }
    }
  } catch (BasicStringBuilder<char>::Overflow) {
  }
}
