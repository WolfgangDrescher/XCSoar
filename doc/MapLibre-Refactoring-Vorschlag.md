# Vorschlag: Karten-Rendering mit MapLibre Native

Status: Variante A in Umsetzung (siehe Abschnitt 8)
Branch: `claude/maplibre-karten-refactoring-hz9c0w`

Dieses Dokument beschreibt, wie das Karten-Rendering von XCSoar schrittweise
auf [MapLibre Native](https://github.com/maplibre/maplibre-native) umgestellt
werden könnte. Es basiert auf einer Analyse der bestehenden Rendering-Pipeline
dieses Forks (inkl. der Fork-Erweiterungen MBTiles-Overlays, EDL/xctherm,
Konturlinien, dynamische Terrain-Quantisierung).

---

## 1. Motivation

Das heutige Rendering ist ein vollständig selbstgebauter Stack:

- CPU-seitige Terrain-Rasterisierung (JPEG2000-DEM via gepatchtem JasPer,
  `RasterRenderer` erzeugt pro Frame ein `RawBitmap`, das als eine einzige
  Textur hochgeladen wird),
- Topographie aus ESRI-Shapefiles (vendored MapServer-Shapelib, eigene
  Triangulierung, eigene Thinning-Level, VBOs pro Layer),
- ein kleines eigenes GLES2-Shader-Set und eine immediate-mode `Canvas`-API.

Das funktioniert, hat aber strukturelle Grenzen:

- **Kartenqualität**: kein echtes Vektortile-Rendering, kein modernes
  Label-Placement/Kollisionshandling, keine Styles, kein stufenloses
  Detail-LOD, Terrain-Shading rein auf der CPU (daher die Quantisierungs-
  Tricks bei Interaktion).
- **Wartungslast**: JasPer-Fork, Shapelib-Fork, eigene Triangulierung, eigene
  Tile-Caches — alles Code, den sonst niemand pflegt.
- **Datenökosystem**: `.xcm` ist ein proprietäres Format; Standard-Quellen
  (OpenStreetMap-Vektortiles, Terrain-RGB-DEMs, MBTiles/PMTiles) sind nur
  über Sonderwege (Fork-MBTiles-Overlay) nutzbar.

MapLibre Native löst genau diese Punkte: GPU-Vektortile-Rendering,
Style-Spezifikation, Label-Collision, Hillshading auf der GPU, Raster- und
Vektorquellen, Offline-fähig, BSD-2-lizenziert (kompatibel mit GPLv2), aktive
Community, C++-Core mit Android/iOS/Linux/Windows/macOS-Support.

---

## 2. Ist-Zustand (Kurzfassung)

Der Master-Renderpass steht in `src/MapWindow/MapWindowRender.cpp`
(`MapWindow::Render()`), Reihenfolge sinngemäss:

```
1. Terrain            BackgroundRenderer → RasterRenderer (CPU-Bitmap)
2. RASP-Wetter        RaspRenderer
3. Topographie        (Cached)TopographyRenderer (Shapefile-VBOs)
4. Overlays           MapOverlay (GeoTIFF / MBTiles / EDL / xctherm) [nur GL]
5. NOAA, FinalGlide-Schattierung
6. Airspace           AirspaceRenderer (+ Labels später)
7. Task, Waypoints, Contest
8. Trail, Waves, Thermik
9. Topographie-Labels
10. Glide/Track/Compass/Wind-Symbolik
11. Traffic (FLARM/GLink), eigenes Flugzeug
12. HUD (GlueMapWindow: Thermal Band, Final Glide Bar, MapScale …)
```

Wichtige Eckpunkte:

- **Projektion** (`src/Projection/Projection.cpp`): rotierende, flugzeug-
  zentrierte Flat-Earth-Näherung (Plate Carrée mit `cos(lat)`-Korrektur),
  kontinuierlicher Massstab in px/m, Rotation per Lookup-Table, Integer-
  Pixel-Ausgabe. **Kein** Web Mercator, **keine** diskreten Zoomstufen.
- **GL-Seam**: `ToGLM()` in `src/ui/canvas/opengl/Geo.cpp` baut bereits eine
  `glm::mat4`, mit der Geo-Koordinaten-VBOs direkt gezeichnet werden — das
  ist die natürliche Nahtstelle zu einer MapLibre-Kamera.
- **GL-Kontext**: global und singulär (`src/ui/canvas/opengl/Globals.hpp`),
  Rendering im UI-Thread (`BufferWindow::OnPaint()`); der `DrawThread`
  existiert nur in Nicht-GL-Builds (Kobo, Windows-GDI).
- **Daten**: `.xcm` = ZIP mit `terrain.jp2` (JPEG2000-DEM), Shapefiles +
  `topology.tpl`, Airspace/Waypoints als Text.
- **Plattformen**: GLES2 überall ausser **Kobo** (Framebuffer/Memory-Canvas,
  Graustufen) und **Windows-PC-Default** (GDI); macOS/Windows-GL laufen über
  ANGLE.

---

## 3. Grundsatzentscheidung: Wie tief soll MapLibre integriert werden?

Drei Varianten, mit klarer Empfehlung:

### Variante A — MapLibre als Basemap-Renderer, XCSoar-Overlays bleiben (empfohlen)

MapLibre ersetzt **nur die Schichten 1–4** des Renderpasses (Terrain, Wetter-
Raster, Topographie, Tile-Overlays). Alles Flugtaktische — Airspace, Task,
Waypoints, Trail, Traffic, HUD — wird weiterhin mit der bestehenden
`Canvas`/Renderer-Infrastruktur **über** die MapLibre-Basemap gezeichnet.

- ✅ Minimal-invasiv: die hunderten `GeoToScreen()`-Aufrufe in
  `src/Renderer/*` bleiben unverändert.
- ✅ Die flugspezifische Darstellung (Airspace-Warnfarben, Task-Rendering,
  FLARM-Symbole) ist erprobt und sicherheitsrelevant — nicht anfassen.
- ✅ Der grösste Qualitätsgewinn (Basemap) kommt zuerst.
- ⚠️ Zwei Rendering-Welten müssen pixelgenau dieselbe Kamera teilen
  (siehe Abschnitt 5.1).

### Variante B — Vollumstieg: alles wird MapLibre-Layer

Airspace, Waypoints, Task usw. werden GeoJSON-Sources/Custom-Layer in
MapLibre; die `Canvas`-Map-Renderer entfallen.

- ✅ Ein einziges Rendering-System, Styles für alles.
- ❌ Sehr grosser Umbau (praktisch jeder Renderer), hohes Regressionsrisiko
  bei sicherheitsrelevanter Darstellung, MapLibre-Styles können manche
  XCSoar-Spezialitäten (Final-Glide-Terrain-Schattierung, Thermal Assistant,
  Label-Decluttering-Regeln) nur mit Custom-Layern — dann rendert man doch
  wieder selbst, nur innerhalb von MapLibre.
- → Allenfalls **Langfristziel**, nicht der Einstieg.

### Variante C — MapLibre nur als weiteres Overlay (Status quo plus)

Raster-Tiles via MapLibre als zusätzliches `MapOverlay` über dem Terrain.

- ✅ Trivial integrierbar (das Fork-MBTiles-Overlay tut das im Kern schon).
- ❌ Löst keines der strukturellen Probleme; JasPer/Shapelib bleiben.
- → Nur als **Phase-1-Spike** sinnvoll (siehe Phasenplan).

**Empfehlung: Variante A**, mit Variante C als erstem Machbarkeits-Spike und
Variante B als optionaler, ferner Ausbaustufe.

---

## 4. Zielarchitektur (Variante A)

```
┌────────────────────────────────────────────────────────────┐
│ GlueMapWindow::Render()                                    │
│                                                            │
│  1. MapLibreBasemapRenderer::Draw(canvas, projection)      │
│     ├─ CameraBridge: Projection → mbgl::CameraOptions      │
│     ├─ mbgl::Map / Renderer  → rendert in eigenes FBO      │
│     └─ Komposition des FBO-Textur-Quads in den Backbuffer  │
│                                                            │
│  2. (unverändert) FinalGlideShading, Airspace, Task,       │
│     Waypoints, Trail, Traffic, Labels, HUD via Canvas      │
└────────────────────────────────────────────────────────────┘
```

Neue Komponenten (Vorschlag Verzeichnis `src/MapWindow/MapLibre/`):

| Komponente | Aufgabe |
|---|---|
| `MapLibreBasemapRenderer` | ersetzt im Renderpass `RenderTerrain/RenderRasp/RenderTopography/RenderOverlays`; besitzt `mbgl::Map`, FBO, Komposition |
| `CameraBridge` | übersetzt `WindowProjection` (Zentrum, px/m, ScreenAngle) verlustfrei in `mbgl::CameraOptions` (center, zoom, bearing) und zurück |
| `XcmTileSource` (optional, Übergang) | MapLibre-Custom-Source, die bestehende Daten (`RasterTileCache`, `TopographyStore`) als Tiles serviert — nur falls Alt-`.xcm` weiter unterstützt werden sollen |
| `StyleFactory` | erzeugt MapLibre-Styles aus den bestehenden `TerrainSettings` (Farbrampen aus `TerrainRenderer.cpp` → Hillshade/Color-Relief-Layer, Topo-Farben aus `topology.tpl` → Vektor-Layer-Paint) |
| `GlStateGuard` | sichert/restauriert XCSoars globalen GL-State um `mbgl`-Renderaufrufe herum |

### 4.1 Rendering-Integration: FBO statt Direkt-Rendering

MapLibre soll **in ein eigenes FBO** rendern (Offscreen), dessen Textur dann
als bildschirmfüllendes Quad in den XCSoar-Backbuffer komponiert wird
(analog zu `DrawGeoBitmap()` / `BufferCanvas`, beides existiert schon):

- Sauberste Isolation: MapLibre verwaltet seinen eigenen GL-State; XCSoars
  globaler Zustand (`OpenGL::projection_matrix`, gebundene Shader, Scissor)
  wird nur einmal gesichert/restauriert (`GlStateGuard`).
- Funktioniert identisch auf EGL, GLX, SDL/ANGLE und Android.
- Ermöglicht später weiche Übergänge (Fade beim Stilwechsel) und das
  Weiterverwenden des letzten Frames bei Pan/Zoom (wie heute `scale_buffer`
  im Nicht-GL-Pfad).

Direkt-Rendering in den Backbuffer wäre marginal schneller, koppelt aber
beide GL-Welten eng — nicht empfohlen für den Einstieg.

### 4.2 Kamera-Synchronisation (der kritischste Punkt)

XCSoar: Flat-Earth-Projektion, kontinuierlich in px/m, Rotation track-up,
Integer-Pixel. MapLibre: Web Mercator, kontinuierlicher (!) Zoom-Level,
`bearing` in Grad, Float-Pixel.

Gute Nachrichten:

- MapLibre unterstützt **kontinuierlichen Zoom** und beliebiges `bearing` —
  die diskrete `ScaleList` von XCSoar ist nur UI-Bedienlogik und kann bleiben.
- Umrechnung px/m → Zoom ist geschlossen lösbar:
  `zoom = log2(EarthCircumference · cos(lat) · scale_px_per_m / tileSize)`
  (mit `tileSize = 512` für MapLibre-Styles).
- `bearing = -ScreenAngle` (Vorzeichenkonvention prüfen), `pitch = 0`.

Restfehler: Flat-Earth und Mercator stimmen exakt nur im Projektionszentrum
überein; zum Bildrand hin divergieren sie. Bei typischen Segelflug-Ausschnitten
(≤ 200 km Kante, |lat| ≤ 60°) liegt die Abweichung im Subpixel- bis
Wenig-Pixel-Bereich — relevant wird sie erst bei extremem Zoom-out.
Zwei Optionen, in dieser Reihenfolge zu evaluieren:

1. **Akzeptieren** und die XCSoar-Overlays weiterhin mit der bestehenden
   Projektion zeichnen (Basemap dient der Orientierung; taktische Overlays
   sind ohnehin die massgebliche Geometrie). Beim maximalen Zoom-out
   (heute schon per `OpenGL::max_map_scale` begrenzt) reicht das voraussichtlich.
2. Falls nicht akzeptabel: XCSoars `Projection` **auf Mercator umstellen**
   (`GeoToScreen()` intern über Mercator rechnen). Das ist ein lokal
   begrenzter Eingriff in `src/Projection/Projection.cpp` + `ToGLM()`,
   dessen Auswirkung (Distanz-/Winkelmessungen sind davon unabhängig, die
   laufen über `src/Geo/`) aber sorgfältig getestet werden muss.

Der `CameraBridge` gehören eigene **Unit-Tests** (Roundtrip
`Projection → CameraOptions → Pixelvergleich` an Bildmitte, Ecken, hohen
Breitengraden) — Infrastruktur dafür existiert unter `test/`.

### 4.3 Threading

Im GL-Build rendert XCSoar synchron im UI-Thread — genau das Modell, das
MapLibre Natives `Renderer` braucht (Renderaufrufe auf dem Thread mit
GL-Kontext). Es wird ein eigener `RendererFrontend` implementiert, der:

- `invalidate()`-Callbacks von MapLibre auf `GlueMapWindow::InjectRedraw()`
  mappt (asynchrones Tile-Laden → Neuzeichnen, wie heute schon
  `TerrainThread`/`TopographyThread` das tun),
- Rendern nur innerhalb von `OnPaintBuffer()` ausführt.

MapLibres eigene Worker-Threads (Tile-Parsing, Netz) laufen unabhängig; das
passt zum bestehenden Modell. Der Nicht-GL-`DrawThread`-Pfad bleibt unberührt
(siehe 4.5).

### 4.4 Datenpipeline: von `.xcm` zu Vektortiles

MapLibre spricht MVT-Vektortiles, Raster-Tiles und Terrain-RGB-DEMs — nicht
JPEG2000-DEM + Shapefiles. Empfehlung: **die Daten wandern, nicht der Code.**

Neues Kartenformat (Arbeitstitel `.xcm` v2, weiterhin ein ZIP — der Container
und `src/io/MapFile.cpp` bleiben):

| Member | Inhalt | ersetzt |
|---|---|---|
| `terrain.pmtiles` | Terrain-RGB (oder Terrarium) Raster-DEM-Tiles | `terrain.jp2` + JasPer |
| `topography.pmtiles` | MVT-Vektortiles (Layer: Wasser, Strassen, Bahn, Orte, Städte-Polygone …) | `*.shp` + Shapelib |
| `style.json` | Default-Style (von `StyleFactory` überschreibbar) | `topology.tpl` |
| `airspace.txt`, `waypoints.cup`, `airfields.txt`, `info.txt` | unverändert | — |

Begründung für **PMTiles** (statt MBTiles): eine Datei, kein SQLite-Zugriff
innerhalb eines ZIP nötig (SQLite kann nicht in ZIP-Membern lesen — die Tiles
müssten sonst entpackt werden), effizientes Random-Access-Format, von
MapLibre Native unterstützt bzw. mit kleinem eigenen `FileSource` anbindbar.
Alternative: `.xcm` v2 nicht als ZIP, sondern als Verzeichnis/Bundle — dann
ginge auch MBTiles direkt (der Fork hat mit `MbTilesDatabase` bereits
funktionierenden Code dafür).

Werkzeuge: Der Map-Generator (xcsoar-mapgen) erzeugt heute JP2+SHP aus
denselben Quelldaten (SRTM/OSM). Umstellung auf `tippecanoe`/`planetiler`
(MVT) und `rio rgbify` o. ä. (Terrain-RGB) ist Standard-Tooling. Für die
Übergangszeit können bestehende `.xcm`-Dateien clientseitig weiter gelesen
werden (via `XcmTileSource`-Adapter) — oder man bietet Neu-Downloads an und
hält den alten Renderpfad hinter einem Build-/Laufzeit-Flag.

Bonus: Online-Quellen (OSM-Vektortiles, Satellit, OpenAIP-Raster) werden
damit "gratis" konfigurierbar — Download/Cache-Infrastruktur (curl,
Coroutinen, SQLite) existiert im Fork bereits für EDL/xctherm.

### 4.5 Plattformen und Fallback

| Plattform | Heute | Mit MapLibre |
|---|---|---|
| Android | GLES2 | ✅ MapLibre-Kernplattform (OpenGL/Vulkan) |
| Linux/UNIX, RPi | GLES2/EGL | ✅ unterstützt |
| iOS/macOS | GLES2 / ANGLE | ✅ (macOS ggf. Umstieg auf Metal-Backend statt ANGLE prüfen) |
| Windows | GDI (Default), GL via ANGLE | MapLibre nur in den GL-Targets (`WIN64OPENGL` …); GDI bleibt Alt-Renderer |
| **Kobo** | Memory-Canvas, kein GL | ❌ MapLibre nicht machbar → **Alt-Renderpfad bleibt** |

Konsequenz: Der bestehende Terrain/Topo-Renderer wird **nicht gelöscht**,
sondern hinter `#ifdef ENABLE_MAPLIBRE` (analog zu `ENABLE_OPENGL`) zur
Fallback-Implementierung. JasPer/Shapelib können erst entfernt werden, wenn
Kobo/GDI entweder aufgegeben oder auf `.xcm` v2 + Software-Rasterizer
umgestellt werden — das ist bewusst **ausserhalb** dieses Refactorings.

### 4.6 Build-Integration

Nach bestehendem Muster:

- `build/libmaplibre.mk` (analog `build/libglm.mk`/`build/libsqlite.mk`),
- Eintrag als `CmakeProject` in `build/python/build/libs.py` für die
  Thirdparty-Targets (Android, Kobo entfällt, PC/mingw, iOS),
- `lib/maplibre/` für Patches/Pins,
- neues Flag `MAPLIBRE ?= y/n` je Target in `build/targets.mk` /
  `build/opengl.mk` (nur wo `OPENGL=y`),
- unter UNIX optional System-/pkg-config-Variante.

Zu klären im Spike: Binärgrösse (MapLibre Native Core + ICU ist erheblich;
für Android relevant), Kompilierzeit, und ob der Vulkan- oder GL-Backend von
MapLibre verwendet wird (GL zuerst, da XCSoar-Kontext GLES2 ist; MapLibre
Native „OpenGL ES 3.0"-Anforderung prüfen — ggf. Mindestanforderung der
GL-Targets anheben, GLES3 ist auf allen relevanten Geräten seit ~2013
verfügbar).

---

## 5. Phasenplan

Jede Phase ist einzeln mergebar und hinter Flags abgesichert.

### Phase 0 — Spike & Entscheidungen (klein)
- MapLibre Native für ein Target (UNIX) bauen, Offscreen-Demo: Karte in FBO
  rendern, Textur in ein GL-Fenster kompositen.
- GLES-Versionsfrage, Binärgrösse, PMTiles-Anbindung klären.
- Ergebnis: Go/No-Go, Festlegung Datenformat (PMTiles vs. MBTiles/Bundle).

### Phase 1 — MapLibre als experimentelles Overlay (Variante C als Vehikel)
- `MapLibreBasemapRenderer` + `GlStateGuard` + `RendererFrontend` bauen,
  aber zunächst als `MapOverlay`-Implementierung einhängen
  (`MapWindow::SetOverlay()`), mit einer Online-Rasterquelle.
- `CameraBridge` inkl. Unit-Tests.
- Sichtbar hinter einem Experimental-Setting; kein Eingriff in den Renderpass.

### Phase 2 — Basemap-Position im Renderpass
- `MapWindowRender.cpp`: bei aktivem MapLibre ersetzen
  `RenderTerrain/RenderRasp/RenderTopography/RenderOverlays` durch
  `maplibre_renderer->Draw()`; sonst Alt-Pfad.
- Redraw-Verkabelung (`InjectRedraw`), Verhalten bei Pan/Zoom/Quick-Redraw,
  Android-Kontextverlust (`GetRenderStateToken()`-Mechanik des Forks nutzen).

### Phase 3 — Datenformat & Generator
- `.xcm` v2 definieren (Abschnitt 4.4), Loader in `src/io/MapFile.*`
  erweitern, `FileSource` für PMTiles-in-ZIP oder Bundle-Verzeichnis.
- Map-Generator-Pipeline auf MVT + Terrain-RGB umstellen.
- `StyleFactory`: Terrain-Farbrampen und `topology.tpl`-Optik als
  MapLibre-Style reproduzieren (inkl. Hillshade-Layer statt CPU-Shading,
  Konturlinien als Vektor-Layer statt `RasterRenderer`-Konturcode).

### Phase 4 — Feature-Parität & Umschalten des Defaults
- RASP/EDL/xctherm-Overlays als MapLibre-Raster-/GeoJSON-Sources einhängen
  (ersetzt die Tile-für-Tile-Bitmap-Zeichnung von `MbTilesOverlay`).
- Topographie-Labels: MapLibre-Symbol-Layer vs. bestehendes `LabelBlock`
  abwägen (MapLibre-Collision ist besser; Interaktion mit XCSoar-eigenen
  Labels — Waypoints, Airspace — beachten: ggf. Topo-Labels in MapLibre,
  Rest weiterhin `LabelBlock`).
- Terrain-Höhenabfragen (`RasterTerrain::GetHeight()` für Final Glide,
  Reach, AGL) **bleiben beim bestehenden Code** — Rendering und Höhenmodell
  werden entkoppelt; das DEM wird dafür weiterhin geladen (aus `.xcm` v2:
  Höhen aus Terrain-RGB-Tiles dekodieren, `RasterMap`-Interface behalten).
- Performance-/Akkutests auf realen Geräten, dann Default umschalten.

### Phase 5 — Aufräumen
- Alten GL-Terrain/Topo-Pfad deprecaten, sobald Kobo/GDI-Strategie
  entschieden ist; erst dann JasPer/Shapelib entfernen.

### Phase 6 (optional, später) — Richtung Variante B
- Einzelne unkritische Layer (z. B. NOAA-Stationen, Distanzringe) als
  MapLibre-Layer, Erfahrung sammeln; taktische Layer nur bei klarem Gewinn.

---

## 6. Risiken & offene Fragen

| Risiko | Einschätzung / Gegenmassnahme |
|---|---|
| GLES2-only-Geräte vs. MapLibre-Anforderungen | Im Spike klären; ggf. Mindestanforderung GLES3 für GL-Targets, Alt-Renderer als Fallback |
| Projektionsdifferenz Flat-Earth ↔ Mercator | Subpixel im Normalbetrieb; Testmatrix in `CameraBridge`-Tests; notfalls `Projection` auf Mercator umstellen |
| Binärgrösse (Android-APK) | Messen im Spike; MapLibre ohne ungenutzte Features bauen |
| Performance auf alten Android-Geräten | MapLibre ist GPU-seitig meist *schneller* als der CPU-`RasterRenderer`; trotzdem Gerätetests vor Default-Umschaltung |
| Doppelte Datenhaltung Übergangszeit (`.xcm` v1+v2) | Alt-Renderpfad bleibt als Fallback; klarer Sunset-Plan |
| Kobo / Windows-GDI | explizit ausserhalb des Scopes; Alt-Pfad bleibt |
| Höhenmodell für Rechnungen (Final Glide, Reach) | bewusst vom Rendering entkoppelt, `RasterMap`-API bleibt stabil |
| Lizenz | MapLibre Native: BSD-2 — kompatibel mit GPLv2; Eintrag in `THIRD_PARTY_NOTICES.txt` |

Offene Fragen an die Projekt-Community:

1. Ist der Verbleib von Kobo (E-Ink, kein GL) langfristig gesetzt?
2. Wird der Map-Generator (Server-Seite) mit umgestellt, oder soll der
   Client Alt-`.xcm` transkodieren können (`XcmTileSource`)?
3. PMTiles-in-ZIP vs. Bundle-Verzeichnis als `.xcm` v2?

---

## 7. Zusammenfassung

- **Variante A**: MapLibre ersetzt die Basemap (Terrain, Topographie,
  Raster-Overlays); alle taktischen Overlays und das HUD bleiben auf der
  bewährten `Canvas`-Pipeline.
- Integration über **Offscreen-FBO + Komposition**, Kamera-Kopplung über eine
  getestete **`CameraBridge`** (`Projection` ↔ `CameraOptions`).
- **Daten wandern**: `.xcm` v2 mit Terrain-RGB- und MVT-Tiles (PMTiles),
  Styles statt `topology.tpl`; Höhenmodell-API (`RasterMap`) bleibt für die
  Flugrechnungen stabil.
- **Alt-Renderer bleibt Fallback** (Kobo, GDI, Alt-Karten), Abbau erst in
  einer späteren Phase.
- Sechs mergebare Phasen, beginnend mit einem kleinen Spike, der die zwei
  echten Unbekannten (GLES-Version, Binärgrösse/PMTiles) klärt.

---

## 8. Implementierungsstand (Variante A)

Umgesetzt auf diesem Branch (`src/MapWindow/MapLibre/`):

| Komponente | Datei | Status |
|---|---|---|
| `MapLibre::Camera` + `CameraFromProjection()` | `Camera.hpp`, `CameraBridge.{hpp,cpp}` | fertig, mbgl-frei, wird immer mitkompiliert |
| Unit-Test der Kamera-Kopplung | `test/src/TestCameraBridge.cpp` | fertig; 71 Checks, misst die Flat-Earth↔Mercator-Abweichung (≤2 px @5 km, ≤8 px @50 km, ≤25 px am Rand eines 500-km-Ausschnitts) |
| `GlStateGuard` | `GlStateGuard.{hpp,cpp}` | fertig (inkl. VAO-Unbind via dynamischem `glBindVertexArray`) |
| `BasemapRenderer` (mbgl::Map, FBO-Backend, Frontend, RunLoop-Pump) | `BasemapRenderer.{hpp,cpp}` | implementiert und gegen die MapLibre-Native-Header typgeprüft; Laufzeittest steht aus, bis MapLibre Native gebaut/verlinkt wird (Phase 0-Spike) |
| Renderpass-Integration | `MapWindowRender.cpp` | fertig: ersetzt `RenderTerrain`+`RenderTopography` hinter `ENABLE_MAPLIBRE` + Laufzeit-Setting; RASP/Overlays/Labels bleiben |
| Settings | `MapSettings` (`maplibre_enabled`, `maplibre_style_url`), Profile-Keys `MapLibreEnabled`/`MapLibreStyleURL` | fertig |
| Konfigurations-UI | Checkbox „MapLibre basemap" (Expert-Row) in Einstellungen → Karte → Map Display | fertig; Style-URL weiterhin nur per Profil-Key |
| Build-System | `build/libmaplibre.mk` (`MAPLIBRE=y`, pkg-config oder `MAPLIBRE_PREFIX`), Einbindung in `build/mapwindow.mk` | fertig; Default `MAPLIBRE=n` |
| MapLibre-Native-Build | `build/maplibre.sh` + Stamp-Regel in `build/libmaplibre.mk` (Muster wie ANGLE, `build/angle.mk`): baut die gepinnte Revision automatisch nach `$(TARGET_OUTPUT_DIR)/maplibre`, wenn kein externes `MAPLIBRE_PREFIX` gesetzt ist. Git-Clone statt Tarball, weil Upstream keine Quell-Tarballs mit Submodulen veröffentlicht (daher kein `libs.py`-Projekt möglich). Host-Targets (UNIX/MACOS/OSX64); Cross-Targets (Android/iOS/Kobo/Windows) brechen mit klarer Meldung ab, bis die Toolchain-Anbindung ergänzt ist. Benötigt MapLibre mit `MLN_WITH_RTTI=ON` (XCSoar baut mit RTTI); unter Linux zusätzlich System-Libs webp/uv/icu | UNIX: Build+Link end-to-end verifiziert (Laufzeittest am Bildschirm ausstehend); macOS: ungetestet |
| macOS/Xcode | `darwin/build.sh` setzt `MAPLIBRE=y` für den MACOS-Target dauerhaft; der Library-Build läuft über die Make-Regel | iOS bewusst noch ohne MapLibre |

Noch offen (nächste Schritte):

1. **Phase 0-Spike abschliessen**: den `darwin/build-maplibre.sh`-Build
   auf einem Mac einmal end-to-end durchziehen (MapLibre-GL-Renderer auf
   XCSoars ANGLE-Kontext ist Neuland) bzw. alternativ zuerst unter
   Linux/UNIX testen; GLES-Version und Binärgrösse messen.
2. Danach Phasen 3-5 gemäss Plan (Datenformat, StyleFactory, Default).
