// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxes/Content/Type.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <type_traits>

/**
 * One thing which the status bar shows: a symbol, or the value of an
 * InfoBox.
 */
struct StatusBarItem {
  enum class Symbol : uint8_t {
    BATTERY,
    GPS,
    FLARM,
    BALLAST,

    /**
     * A red dot while the logger records.
     */
    LOGGER,

    /**
     * Whether there is a connection to the internet.
     */
    NETWORK,

    /**
     * The position of the page, e.g. "2/5".
     */
    PAGE_NUMBER,

    /**
     * The short title of the page.
     */
    PAGE_TITLE,

    /**
     * Simulator or replay, if the data does not come from the GPS.
     */
    DATA_SOURCE,

    COUNT
  };

  /**
   * The part of the status bar in which an item sits.  A middle zone
   * may follow one day.
   */
  enum class Zone : uint8_t {
    LEFT,
    RIGHT,

    COUNT
  };

  enum class Kind : uint8_t {
    SYMBOL,
    INFOBOX,
  };

  Zone zone;

  Kind kind;

  /**
   * A #Symbol or an #InfoBoxFactory::Type, depending on #kind.
   */
  uint8_t id;

  static constexpr StatusBarItem Make(Zone zone, Symbol symbol) noexcept {
    return {zone, Kind::SYMBOL, uint8_t(symbol)};
  }

  static constexpr StatusBarItem Make(Zone zone,
                                      InfoBoxFactory::Type type) noexcept {
    return {zone, Kind::INFOBOX, uint8_t(type)};
  }

  constexpr bool operator==(const StatusBarItem &) const noexcept = default;

  constexpr bool IsSymbol(Symbol symbol) const noexcept {
    return kind == Kind::SYMBOL && id == uint8_t(symbol);
  }
};

/**
 * The items of the status bar, zone by zone, each zone from left to
 * right.
 */
struct StatusBarItems {
  /**
   * More than fit on any screen: a technical limit, not a design
   * one.
   */
  static constexpr unsigned MAX_ITEMS = 32;

  /**
   * Sorted by zone.
   */
  std::array<StatusBarItem, MAX_ITEMS> items;
  unsigned n_items;

  void SetDefaults() noexcept;

  void Clear() noexcept {
    n_items = 0;
  }

  bool IsFull() const noexcept {
    return n_items >= MAX_ITEMS;
  }

  /**
   * Insert an item at the given index, which must keep the items
   * sorted by zone.
   */
  void Insert(unsigned i, StatusBarItem item) noexcept;

  /**
   * Append an item at the end of its zone.
   */
  void Append(StatusBarItem item) noexcept;

  void Remove(unsigned i) noexcept;

  std::span<const StatusBarItem> GetItems() const noexcept {
    return {items.data(), n_items};
  }

  /**
   * The index of the first item of the given zone, or where it would
   * be.
   */
  [[gnu::pure]]
  unsigned GetZoneBegin(StatusBarItem::Zone zone) const noexcept;

  [[gnu::pure]]
  std::span<const StatusBarItem>
  GetZone(StatusBarItem::Zone zone) const noexcept;

  [[gnu::pure]]
  bool Contains(StatusBarItem::Symbol symbol) const noexcept {
    for (const auto &item : GetItems())
      if (item.IsSymbol(symbol))
        return true;
    return false;
  }

  constexpr bool operator==(const StatusBarItems &other) const noexcept {
    if (n_items != other.n_items)
      return false;
    for (unsigned i = 0; i < n_items; ++i)
      if (items[i] != other.items[i])
        return false;
    return true;
  }

  /**
   * Parse a list as written by Format(), e.g.
   * "left=page,title;right=infobox:52,gps".  Unknown zones and items
   * are skipped.
   *
   * @return false if there was no known zone, e.g. in an older format
   */
  bool Parse(const char *s) noexcept;

  /**
   * Write the list to the given buffer for the profile.
   */
  void Format(std::span<char> buffer) const noexcept;
};

static_assert(std::is_trivial<StatusBarItems>::value, "type is not trivial");
