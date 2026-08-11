# Custom screen layouts for XCSoar: design draft (RFC)

Follow-up to #2849 and the feedback there, building on #1863 (per-page InfoBox geometry) and the "Custom (JSON file)" InfoBox geometry prototype. This document tries to cover all customization aspects raised in the discussion: freely placeable and extensible screen elements ("MapGlueElements"), new element types (waypoint guide, altitude/speed tape, vario bar), and the required rework of the layout maths. It is a draft meant to be torn apart and refined by the community.

## 1. Goal

A pilot should be able to describe a complete flight display page in a layout file: which elements appear, where, how big, and (for InfoBoxes) with which content. One layout per InfoBox set, switching automatically with the flight phase. Built-in geometries stay untouched and remain the default; custom layouts are strictly opt-in with a safe fallback.

## 2. Current state: inventory of screen elements

Today's screen is composed of elements with three very different placement mechanisms.

**Class A: placed by InfoBoxLayout.** The InfoBoxes and the map. `InfoBoxLayout::Calculate()` (src/InfoBoxes/InfoBoxLayout.cpp) turns a geometry enum into box rects plus the `remaining` rect for the map; a hardcoded switch per geometry. The prototype already generalizes this class: box rects from a JSON file (absolute in a reference screen space with proportional scaling, or percent of the actual screen, mixable), optional vario area, map area explicit or computed as the largest free rectangle.

**Class B: separate windows with their own placement settings.** These are real child windows of MainWindow, so free placement is technically cheap; each one currently has its own ad-hoc mechanism:

| Element | Current placement |
|---|---|
| Vario gauge (`GaugeVario`) | `Layout::vario` rect, only set by a few geometries |
| FLARM radar (`GaugeFLARM`) | `TrafficSettings::GaugeLocation` corner enum + "auto" |
| Thermal assistant | `TAPosition` setting (corners) |
| Menu / zoom / QuickMenu buttons | fixed corners in MainWindow |
| Popup messages (`PopupMessage`) | setting: centered or top-left |
| Bottom area (cross section etc.) | `PageLayout::Bottom`, fixed strip below the map |
| Menu bar (`MenuBar`) | fixed grid at the screen edges |

**Class C: painted onto the map canvas ("MapGlueElements").** `GlueMapWindow::OnPaint` (src/MapWindow/GlueMapWindowEvents.cpp, ~line 694-781) draws these directly, each with a position hardcoded in its draw method or renderer: final glide bar (`FinalGlideBarRenderer`, left center), vario bar (`VarioBarRenderer`, right edge), thermal band (top left, `rc.left + Layout::Scale(25)` wide, 1/5 of the map high), map scale + map title (bottom left), flight mode icon (bottom right), GPS status, pan info, crosshairs. They are cheap to draw, flicker-free on e-ink, and can be transparent over the map, but their rects are not configurable at all today.

## 3. Proposed element model

One generic model for all three classes. A layout file contains a list of typed elements:

```json
{
  "version": 2,
  "screen": { "width": 800, "height": 480, "dpi": 96 },
  "strict": false,
  "elements": [
    { "type": "map", "x": 0, "y": 0, "width": "100%", "height": "100%" },
    { "type": "infobox", "x": 650, "y": 416, "width": 150, "height": 64, "content": 10, "border": ["top"] },
    { "type": "vario-gauge", "x": "92%", "y": "20%", "width": "8%", "height": "60%" },
    { "type": "final-glide-bar", "anchor": "left-center", "height": "40%" },
    { "type": "flarm-radar", "anchor": "bottom-right", "width": 120, "height": 120, "margin": 8 }
  ]
}
```

Common attributes for every element:

- `type`: the element kind (catalog below)
- geometry: either `x`/`y`/`width`/`height` (number = pixels in reference space, scaled proportionally; string `"20%"` = percent of actual screen; mixable), or an `anchor` (`top-left`, `top-center`, ..., `center`, edge centers) with `width`/`height`/`margin` for elements that should stick to an edge instead of a fixed spot
- `z`: optional stacking order for overlapping window elements (painted map overlays always stay below windows)
- `show`: optional visibility conditions, e.g. `"circling"`, `"cruise"`, `"final-glide"`, `"pan"`, mirroring the auto rules that exist today (thermal band only while relevant, FLARM auto, etc.); default keeps today's behavior per type
- type-specific attributes: `content`/`border` for InfoBoxes, gauge options later

The v1 prototype format (`"boxes"` array) stays valid as a shorthand for a list of `infobox` elements plus optional `vario`/`map`; the loader translates it internally.

## 4. Element catalog

**Available with the current prototype:** `infobox` (with content pinning via the `InfoBoxFactory::Type` id, same as the `InfoBoxPanel<n>Box<i>` profile keys), `map`, `vario-gauge`.

**Class B promotion (mostly plumbing):** `flarm-radar`, `thermal-assistant`, `bottom-widget` (cross section), `menu-button`, `zoom-buttons`, `quickmenu-button`. These already are windows; the work is routing their rects through the layout engine instead of their private settings. Their existing settings become the defaults when a layout does not mention them.

**Class C promotion (the actual "MapGlueElements" work):** `final-glide-bar`, `vario-bar`, `thermal-band`, `map-scale`, `flight-mode-icon`, `gps-status`. Proposal: keep them painted inside the map canvas (preserves transparency, draw order and e-ink friendliness) but have `GlueMapWindow` receive a small table of resolved rects from the layout engine instead of computing them inline. The renderers already take a `PixelRect`, so this is a contained refactor per element. Constraint to document: these elements must lie within the map area; placing them outside the map requires promoting the element to a window (possible later, per element, without changing the file format).

**New elements (community suggestions, each is a new renderer plus a catalog entry):**

- `waypoint-guide`: a course/next-waypoint guidance strip (name, bearing chevrons, distance)
- `altitude-tape` and `speed-tape`: PFD-style vertical tapes; natural candidates for the left/right screen edges
- `vario-bar` as a standalone element also outside the map (today it exists only as a map overlay)

The catalog is intentionally open: adding an element type means adding a renderer (or reusing one) and registering a type name, without touching the file format.

## 5. Layout maths rework

The current `InfoBoxLayout::Calculate()` is a per-geometry switch; the prototype added a second path for custom files. Proposed target structure:

1. **Resolve pass**: for each element, resolve its geometry to a `PixelRect` in screen space. Inputs: reference screen size, actual screen size, DPI, the px/percent/anchor rules above. Deterministic and side-effect free, so it stays unit-testable (the prototype already has standalone tests for scaling, percent, strict fallback and the map computation).
2. **Map default**: if no `map` element is present, the largest axis-aligned rectangle not covered by any opaque element becomes the map (implemented and proven in the prototype; maximal-rectangle search over the compressed coordinate grid).
3. **Validation and fallback**: files are validated at load time with precise error messages (unknown type, bad content id, out-of-range coordinates); any failure falls back to the selected built-in geometry, never to a broken screen. `strict: true` additionally requires an exact screen match.
4. **Selection**: per InfoBox set (one file per set, global default file as fallback), integrating with the per-page geometry override from #1863: a set whose geometry resolves to "Custom" uses its assigned file. Re-layout happens on set/page switches, as prototyped.
5. **Out of scope of the resolve pass**: modal dialogs, the configuration screens and the menu system remain unchanged.

Deliberately not proposed: a general constraint solver (grid/flexbox). Absolute + percent + edge anchors cover the LXNAV-styler use cases with predictable results and a testable implementation; a solver can be revisited if real layouts prove it necessary.

## 6. Interaction and UX rules

- InfoBoxes keep their tap behavior; boxes with pinned `content` refuse the picker with a short notice (prototyped).
- Touch: the editor and the loader should warn when an interactive element is smaller than the platform's minimum touch target.
- Popup messages and status texts: get a `popup-message` placement policy element (position only, size stays content-driven); when absent, today's setting applies.
- Overlapping windows are allowed (HUD style, prototyped); painted map overlays cannot overlap windows by construction.
- E-ink: window moves are cheap, but layouts with many overlapping windows may ghost; the docs should note this, no hard restriction.

## 7. Compatibility and migration

- Built-in geometries remain the default and are untouched; long-term they could be shipped as bundled layout files, but that is optional and not part of this proposal.
- Profile: one key for the default layout file, one per InfoBox set (`InfoBoxPanel<n>CustomGeometryFile`, prototyped), coexisting with #1863's `InfoBoxPanel<n>Geometry`.
- A v1 file keeps working under v2 unchanged; `version` gates future extensions.

## 8. Tooling: web-based layout editor

The LXNAV styler is Windows-only; the editor for this format should be a web app. Publishing a JSON schema for the format enables validation in the editor and third-party tooling. Ideally the editor is combined with a docs rework so the InfoBox reference (all values, descriptions, ids) lives next to it: click a page together in the browser, pick contents from the reference, download the JSON into XCSoarData. XCSoar itself only needs a small addition: exporting the currently active layout as a JSON file, as a starting point for editing.

## 9. Suggested phases

1. **Phase 1 (exists as prototype):** `infobox`, `map`, `vario-gauge` via JSON; per-set files; scaling/strict; content pinning; fallback; docs.
2. **Phase 2:** element list format (`version: 2`), class B windows placeable (`flarm-radar`, `thermal-assistant`, `bottom-widget`, buttons), `show` conditions, JSON schema.
3. **Phase 3:** class C rect routing (`final-glide-bar`, `vario-bar`, `thermal-band`, `map-scale`, ...), layout export from XCSoar.
4. **Phase 4:** new renderers: `waypoint-guide`, `altitude-tape`, `speed-tape`.
5. **Phase 5:** web editor + docs/InfoBox reference integration.
6. **Later:** styling (per-box colors, fonts), promoting individual map overlays to windows where placement outside the map is wanted.

## 10. Open questions

- Which class C elements really need free placement, and which just need an on/off plus a choice of edge? (Each rect routing is a small refactor; prioritization welcome.)
- Should `show` conditions be per element in the file, or should flight-phase differences be expressed only via per-set files? (Both work; per-set files are simpler, `show` is more compact.)
- Font sizing for InfoBoxes of very different sizes: one font per layout (smallest box wins, prototyped) or per-box font scaling?
- How should the bottom widget (cross section) interact with a free map rect: keep as separate element or as a map option?
- Minimum sanity rules for the editor vs. the loader: how much should XCSoar itself validate (overlaps, touch targets) beyond parse errors?
