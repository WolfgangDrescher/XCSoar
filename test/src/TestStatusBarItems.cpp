// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "StatusBarItems.hpp"
#include "util/StringAPI.hxx"
#include "TestUtil.hpp"

#include <stdio.h>
#include <string.h>

using Symbol = StatusBarItem::Symbol;
using Zone = StatusBarItem::Zone;

static bool
FormatsAs(const StatusBarItems &items, const char *expected)
{
  char buffer[256];
  items.Format(std::span{buffer});
  return StringIsEqual(buffer, expected);
}

int main()
{
  plan_tests(20);

  StatusBarItems items;
  items.SetDefaults();
  char expected[256];
  snprintf(expected, sizeof(expected),
           "left=source,page,title;"
           "right=infobox:%u,logger,ballast,flarm,gps,network,battery",
           unsigned(InfoBoxFactory::e_TimeLocal));
  ok1(FormatsAs(items, expected));

  /* round trip */
  StatusBarItems parsed;
  char buffer[256];
  items.Format(std::span{buffer});
  parsed.Parse(buffer);
  ok1(parsed == items);

  /* the order within a zone is kept, the zones are sorted */
  parsed.Parse("right=battery,infobox:0;left=gps");
  ok1(parsed.n_items == 3);
  ok1(parsed.items[0] == StatusBarItem::Make(Zone::LEFT, Symbol::GPS));
  ok1(parsed.items[1] == StatusBarItem::Make(Zone::RIGHT, Symbol::BATTERY));
  ok1(parsed.items[2] ==
      StatusBarItem::Make(Zone::RIGHT, InfoBoxFactory::Type(0)));
  ok1(FormatsAs(parsed, "left=gps;right=battery,infobox:0"));

  /* the zones */
  ok1(parsed.GetZone(Zone::LEFT).size() == 1);
  ok1(parsed.GetZone(Zone::RIGHT).size() == 2);
  ok1(parsed.GetZoneBegin(Zone::RIGHT) == 1);

  /* unknown zones and items are skipped */
  parsed.Parse("middle=gps;right=wifi,gps,infobox:,infobox:x,"
               "infobox:9999,,flarm;nonsense");
  ok1(FormatsAs(parsed, "left=;right=gps,flarm"));

  /* no known zone: e.g. an older format */
  ok1(!parsed.Parse("page,title,gps"));
  ok1(parsed.Parse("left="));

  /* an empty list */
  parsed.Parse("");
  ok1(parsed.n_items == 0);
  ok1(FormatsAs(parsed, "left=;right="));

  /* no more than MAX_ITEMS */
  {
    char many[1024] = "right=gps";
    for (unsigned i = 0; i < StatusBarItems::MAX_ITEMS; ++i)
      strcat(many, ",gps");
    parsed.Parse(many);
    ok1(parsed.n_items == StatusBarItems::MAX_ITEMS);
  }

  /* appending goes to the end of the zone */
  parsed.Parse("left=page;right=gps");
  parsed.Append(StatusBarItem::Make(Zone::LEFT, Symbol::PAGE_TITLE));
  ok1(FormatsAs(parsed, "left=page,title;right=gps"));

  /* removing keeps the order of the rest */
  parsed.Remove(1);
  ok1(FormatsAs(parsed, "left=page;right=gps"));
  parsed.Remove(0);
  ok1(FormatsAs(parsed, "left=;right=gps"));
  ok1(parsed.Contains(Symbol::GPS) && !parsed.Contains(Symbol::PAGE_NUMBER));

  return exit_status();
}
