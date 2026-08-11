Custom InfoBox Geometry
=======================

Besides the built-in InfoBox geometries (e.g. "8 Split", "12 Bottom or
Right"), XCSoar supports a free-form, pixel-precise InfoBox layout
loaded from a JSON file, similar in spirit to the LXNAV styler.

To use it:

1. copy a layout file (see the format below) into the ``XCSoarData``
   directory
2. in *Config → System → Look → Screen Layout*, set *InfoBox geometry*
   to *Custom (JSON file)* and select the file in *Custom InfoBox
   geometry file*

If the file cannot be loaded (missing, malformed, or ``strict`` is set
and the screen does not match), XCSoar falls back to the default "8
Split" geometry and logs the reason.

File format
-----------

.. code-block:: json

   {
     "screen": { "width": 800, "height": 480, "dpi": 120 },
     "strict": false,
     "vario": { "x": 640, "y": 0, "width": 160, "height": 120 },
     "map": { "x": 0, "y": 0, "width": 640, "height": 480 },
     "boxes": [
       { "x": 640, "y": 120, "width": 160, "height": 90, "content": 10 },
       { "x": 640, "y": 210, "width": 160, "height": 90,
         "border": ["top", "left"] },
       { "x": "80%", "y": "62.5%", "width": "20%", "height": "18.75%" },
       { "x": 640, "y": 390, "width": 160, "height": 90 }
     ]
   }

``screen`` (required)
  The screen resolution this layout was designed for.  ``width`` and
  ``height`` are in pixels; ``dpi`` is optional and refers to XCSoar's
  *Display resolution (DPI)* setting.

``strict`` (optional, default ``false``)
  When ``true``, the layout is only applied when the actual screen
  size matches ``screen`` exactly (and, if ``dpi`` is given, the
  profile's custom DPI setting matches too).  Otherwise XCSoar falls
  back to the default geometry.

  When ``false``, absolute pixel coordinates are scaled
  proportionally from the reference screen size to the actual screen
  size, so the same file works on differently sized screens.

``boxes`` (required, 1..24 entries)
  One entry per InfoBox, in InfoBox set order (the first entry is
  InfoBox 1, etc.).  Each entry has:

  ``x``, ``y``, ``width``, ``height`` (required)
    Either a number (pixels in the reference screen space defined by
    ``screen``) or a percentage string such as ``"12.5%"`` (relative
    to the actual screen size, hence resolution-independent).  Both
    styles can be mixed freely, even within one box.

  ``border`` (optional)
    An array of edges to draw when the InfoBox border style is "Box":
    any of ``"top"``, ``"right"``, ``"bottom"``, ``"left"``.  The
    default is all four edges.  Use ``[]`` to disable the border.

  ``content`` (optional)
    Pin the content shown in this box, as an integer
    ``InfoBoxFactory::Type`` value — the same identifier that the
    profile stores in the ``InfoBoxPanel<n>Box<i>`` keys (see
    ``src/InfoBoxes/Content/Type.hpp``; e.g. ``10`` = MacCready
    setting).  A pinned box always shows this content, regardless of
    the active InfoBox set, and cannot be changed with the InfoBox
    picker.  Boxes without ``content`` keep showing the active set's
    content for their position.  With per-set layout files this
    allows a single file to describe a page completely — placement
    and contents.

``vario`` (optional)
  The area for the vario gauge, with the same ``x``/``y``/``width``/
  ``height`` semantics as a box.

``map`` (optional)
  The area for the map.  When omitted, XCSoar uses the largest screen
  rectangle not covered by any InfoBox (or the vario gauge).  Specify
  it explicitly if you want InfoBoxes to float on top of a
  full-screen map, e.g. ``{ "x": 0, "y": 0, "width": "100%",
  "height": "100%" }`` — note that ``map`` itself may also use
  percentages.

Per-set layout files
--------------------

Besides the global custom geometry file, each InfoBox set (panel —
Circling, Cruise, FinalGlide, AUX-…) can have its own layout file:
open *Config → System → Look → InfoBox Sets*, select a set and choose
a *Geometry file* there.  When the InfoBox geometry is *Custom
(JSON file)*, XCSoar uses the set's own file while that set is
active, falling back to the global file for sets without one.  The
layout switches automatically when the active set changes (e.g.
circling/cruise auto-switch or page changes).

The per-set files are stored in the profile under
``InfoBoxPanel<n>CustomGeometryFile`` (``<n>`` = 0..7), analogous to
the other per-panel keys.

This is designed to stay compatible with per-page InfoBox geometry
overrides (the ``infobox-geometry-per-page`` work): once a set can
override the geometry *type* as well, a set whose geometry resolves
to *Custom* will keep using its assigned layout file.

Notes
-----

- The number of entries in ``boxes`` determines how many InfoBoxes
  are shown; their contents still come from the regular InfoBox sets
  (*Config → System → Look → InfoBox Sets*).
- InfoBox fonts are auto-sized for the smallest box, so text fits in
  every box.
- A future InfoBox configurator is planned to create these files
  interactively; until then they can be written by hand.
