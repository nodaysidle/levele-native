# HANDOFF — Levele (native)

`/home/arch/dev/nodaysidle/levele-native`

A native **C++17 + Qt Quick 6.12** rewrite of the Levele design-studio web app
(`/home/arch/dev/nodaysidle/levele`, Next.js + Konva). One binary, built with
`qmake6 && make`, installed per-user, themed live from the Omarchy desktop theme.

Written 2026-11-17 for Alan Pfeiffer (@nodaysidle) on Arch/Omarchy/Hyprland/Wayland.
Status: **working and installed**. Not a git repo yet — nothing has been committed.

---

## 1. TL;DR — read this, then build and run

```sh
cd /home/arch/dev/nodaysidle/levele-native
./bin/build                         # single binary at build/levele
./bin/test                          # builds + --selftest + --theme-sweep
./bin/install                       # ~/.local/bin/levele + icon + menu entry
levele                              # or launch from the Omarchy menu: "Levele"
```

Verify without a human (all three exit 0 on a healthy build):

```sh
./bin/test                                                  # --selftest (15 templates) + --theme-sweep (89 themes)
timeout 3 ~/.local/bin/levele; echo $?                      # 124 = alive at timeout = clean launch
```

There is no test suite. Those two flags are the test suite.

---

## 2. What it is

A layered design studio: pick or create a page, drop shapes / text / images on a
canvas, arrange them in a layer list, edit properties in an inspector, export.

| Area | Implemented |
|---|---|
| Document | size presets (7), multi-page, per-page background, JSON persistence |
| Elements | rect, rounded rect, ellipse, triangle, star, line, arrow, text, image |
| Canvas | zoom 5–400 %, pointer-anchored Ctrl+wheel, pan, grid, snapping with guides, marquee |
| Direct manipulation | move, 8-handle resize, rotate, arrow-key nudge, inline text editing |
| Layers | visibility, lock, rename, drag-reorder, front/raise/lower/back |
| Inspector | position/size/rotation/opacity, fill + gradient, stroke, radius, type settings, align, distribute |
| Pages | add / duplicate / delete / reorder, live thumbnails |
| Templates | 15 real designs (matching legacy `levele`; 2 of them multi-page) |
| Shortcuts | global `Ctrl+Z/Shift+Z/S/E/D/A/0/+/-`, `Esc`, `?` (shortcuts overlay), `Q` (save & quit) |
| History | snapshot undo/redo, 100 steps |
| Persistence | autosave to `~/.local/share/levele/docs/<id>.json`, dashboard "recent" list |
| Export | **PNG** (1–4×), **PDF** (all pages, millimetre-accurate), **GIF** (all pages, animated, looping) |
| Theming | reads the live Omarchy theme + Hyprland `decoration:rounding` (`theme.radius`), follows switches live |

Not implemented — see §11.

---

## 3. Provenance

The web app was inspected first and used as the feature spec. What carried over:
the element model, snapping, the shape set, the inspector field set, the template
idea, and the export options. What was deliberately dropped: MQTT live rooms, share
links, comments, accounts, IndexedDB, Yjs, and WebM video export — none of them have
a sensible single-process Qt counterpart, and video has no Qt encoder.

The GIF exporter is original: **Qt can read GIFs but cannot write one** (`libqgif.so`
is read-only), so `src/exporter.cpp` contains a hand-written GIF89a encoder
(median-cut palette + LZW). Do not "simplify" it away.

---

## 4. Architecture

```
main.cpp
  ├─ Theme            (QObject, context property "theme")   ← reads colors.toml, watches it
  ├─ Studio           (QObject, context property "studio")  ← document, layers, undo, IO, export
  │    └─ ElementListModel (QAbstractListModel)             ← rows = layers of current page
  └─ QQmlApplicationEngine → qrc:/qml/Main.qml
        Main.qml → Dashboard.qml | Editor.qml
                        └─ CanvasStage.qml → CanvasItem (C++ QQuickPaintedItem)
                              paint() calls Exporter::paintPage()  ← same code as PNG/PDF/GIF
```

Two rules keep this coherent:

1. **One renderer.** `Exporter::paintPage(painter, doc, pageIndex, targetRect)` draws a
   page at any scale. The screen, the thumbnails, and every export call it. If you add
   an element type or a visual feature, add it *there* and all three stay in sync.
2. **The canvas owns interaction, not QML.** `CanvasItem` receives the mouse/keyboard,
   works in document coordinates, and mutates the document directly. QML only draws
   chrome around it and edits properties through `Studio`.

---

## 5. File map

| File | Lines | Responsibility |
|---|---:|---|
| `src/main.cpp` | 292 | app setup, context properties, `--selftest`, `--theme-sweep`, `--demo` |
| `src/theme.cpp/.h` | 276/80 | Omarchy `colors.toml` parsing, alias lookup, contrast derivation, `QFileSystemWatcher` |
| `src/document.cpp/.h` | 228/89 | `Element` / `Page` / `Doc` structs, JSON round-trip, type labels+glyphs |
| `src/templates.cpp/.h` | 297/27 | 8 templates as real geometry, 7 size presets, `makeBlankDoc` |
| `src/studio.cpp/.h` | 931/179 | `ElementListModel` + `Studio`: selection, props, ops, pages, undo, autosave, doc store, export entry points |
| `src/canvasitem.cpp/.h` | 1049/164 | painting, hit-testing, move/resize/rotate, snapping, marquee, inline text edit, view ops |
| `src/exporter.cpp/.h` | 695/23 | `paintPage`, PNG, PDF, GIF89a encoder (palette + LZW), GIF self-verification |
| `qml/Main.qml` | 104 | window, screen switch (`studio.editing`), toast, global shortcuts |
| `qml/Dashboard.qml` | 284 | header, size presets, template grid with live previews, recent docs |
| `qml/Editor.qml` | 283 | top bar, tool grid + layers (left), inspector (right), canvas, page strip |
| `qml/CanvasStage.qml` | 124 | wraps `CanvasItem`, inline text-edit overlay, zoom chip |
| `qml/LayersPanel.qml` | 249 | layer rows, drag-reorder, visibility/lock, z-order buttons |
| `qml/Inspector.qml` | 351 | Transform / Fill / Text / Image / Arrange / Page sections |
| `qml/PageStrip.qml` | 105 | page thumbnails (live), add/delete/duplicate/switch |
| `qml/ExportDialog.qml` | 172 | PNG/PDF/GIF options + save dialog |
| `qml/ShortcutsOverlay.qml` | 99 | `?` keyboard shortcuts overlay |
| `qml/ui/*.qml` | 333 | themed `Btn`, `NumField`, `ColorChip`, `Combo`, `Section`, `Caption`, `Toggle` |
| `bin/{build,test,install}` | — | out-of-tree `build/levele`, offscreen test runner, user install wrapper |
| `resources.qrc` | — | every QML file + the SVG, compiled into the binary |
| `install.sh` | — | binary, SVG + 8 PNG sizes, `.desktop`, cache refresh |

All QML and the icon are embedded via `resources.qrc`, so the binary is relocatable.
**If you add a QML file you must add it to `resources.qrc`** or it will not load.

---

## 6. Theme engine (the part most likely to break)

Live file: `~/.local/state/omarchy/current/theme/colors.toml`
(`~/.config/omarchy/current` is a symlink to `.local/state/omarchy/current`; both are
supported, state path preferred). Per-theme fallback: `<dir>/theme/colors.toml`, then
`<dir>/colors.toml`.

- **Keys are not standardised across themes.** `Theme` reads through alias lists
  (`background`/`bg`, `foreground`/`fg`, `lighter_background`/`lighter_bg`,
  `dark_background`/`dark_bg`/`darker_bg`, …). A theme that writes the `*_bg` set will
  lose every surface colour if you only know the `*_background` names.
- **`muted` is a dim background token, not an ink.** Omarchy's `muted` (#2b2a28) is
  unreadable as text. Secondary text is derived from `foreground` instead.
- **Missing roles are mixed from the theme's own colours**, never from a hardcoded
  slate — otherwise it looks right on one theme and wrong on twenty-three.
- **Contrast is enforced, not assumed.** Body/strong/muted/status ink is walked toward
  black or white until it clears 4.5:1 against *both* surface and chrome; borders 3:1.
  The blending uses 8-bit rounded channels on purpose: `QColor::fromRgbF` keeps 16-bit
  channels, so the exported `#rrggbb` disagrees with the measured colour and the shipped
  value lands just under the threshold.
- **The watcher must watch the directory, not the file.** `omarchy-theme-set` replaces
  the whole `current/theme` directory (`rm -rf` + `mv`), so `colors.toml`'s inode changes
  and a file watch dies after one switch. `Theme` watches `…/current` (directoryChanged)
  *and* `theme.name` (fileChanged), then re-adds paths and re-reads, debounced 150 ms.
- `mode` also drives `QStyleHints::setColorScheme` so Qt matches the desktop.

Prove changes with `./levele --theme-sweep` — it loads every installed theme through the
real class and prints `89 themes checked, 0 failing pairs`. If that number regresses, the
derivation broke.

---

## 7. Document model and Studio

`document.h` is plain structs (no QObject) — cheap to copy, easy to serialise.
`Doc::toJson()` is the undo snapshot *and* the save format, so persistence and history
share one code path.

**Units and coordinates**
- Doc coordinates = document pixels, origin top-left. `Element::x/y` is the top-left of
  the *unrotated* box; `rotation` is degrees clockwise about the box centre.
- Line/arrow may carry negative `w`/`h`; `Element::rect()` normalises.
- Screen mapping lives in `CanvasItem`: `screen = pan + doc * zoom`.

**Studio responsibilities** (`studio.cpp`)
- `ElementListModel` exposes the current page's elements as rows; row 0 is the
  bottom-most element (painting order). The layers panel renders it
  `verticalLayoutDirection: BottomToTop` so the top layer reads at the top.
- Selection is a `QStringList` of ids. `sel()` returns a **QVariantMap snapshot** of the
  primary element, and the inspector binds to it. That is why the inspector has no
  per-field C++ plumbing — if you need a new property, add it in `Element`, `toJson`,
  `sel()`, and `setSelProp()`.
- Every mutating invokable calls `pushUndo()` (snapshot before the change) then `touch()`
  (refresh model + schedule autosave + emit `contentsChanged`). Gestures push once at
  press and then call `touchSelection()` while dragging.
- Autosave fires 800 ms after the last change; undo cap is 100.

**Data on disk**

```
~/.local/share/levele/docs/<id>.json     one file per document (undo snapshots are the same shape)
~/.local/share/levele/assets/<id>.<ext>  imported images are COPIED here, so a doc survives
                                         the original file being moved or deleted
```
`XDG_DATA_HOME` is honoured. `--selftest` overrides it so the test never touches real data.

---

## 8. Canvas interaction

State machine in `CanvasItem::mousePressEvent`:

```
middle-button or Space held → Panning
handle hit (8 + rotate)     → Resizing / Rotating
active tool != select       → Creating (or text → add + edit)
click on element            → Moving (all selected, snapped)
click on empty              → Marquee
```

- Hit-testing is top-down and skips invisible/locked elements; rotated elements are
  hit-tested by unrotating the point about the element centre.
- Snapping compares the moving box's edges *and* centre against page edges/centre and
  every other element's edges/centre, at a 7 px screen tolerance (`kSnapPx / zoom`).
  Guides are stored in document space and drawn dashed.
- Resize on a rotated element unrotates the pointer about the original centre, edits the
  axis-aligned box in that local frame, then re-anchors: new centre =
  `rotateAround(newLocalCentre, oldCentre, rotation)`. That keeps the opposite edge
  fixed. Do not "simplify" this to `centreDoc = newLocalCentre` — it drifts.
- Inline text editing is a QML `TextEdit` overlay (`CanvasStage.qml`) positioned from
  `CanvasItem::editRect`. Enter commits, Shift+Enter inserts a newline, Escape cancels.

**Tool shortcuts live in C++, not QML** (`V R O T L G` in `keyPressEvent`) so they only
fire when the canvas has focus. Global Ctrl-shortcuts live in `Main.qml` and are gated on
`studio.typing`, which `main.cpp` maintains from `focusObjectChanged`.

---

## 9. Export

| Format | Scope | Notes |
|---|---|---|
| PNG | current page | scale 1–4×, plain `QImage::save` |
| PDF | all pages | `QPdfWriter` at 96 dpi, page size `px * 25.4 / 96` mm, zero margins |
| GIF | all pages | animated, looping, width + fps configurable |

The GIF encoder is the delicate part:

- median-cut palette (max 256) sampled across all frames, nearest-colour mapping cached
  per unique RGB
- GIF LZW with `minCodeSize = 8`
- **The code width grows at `2^codeSize + 1`, not at `2^codeSize`.** The decoder adds its
  table entry one code later than the encoder, so growing early desyncs every subsequent
  code. Symptom: the header, frame count, size and palette all validate, and every real
  decoder still reports "broken data stream". Growing one code later is correct.
- `writeGif()` re-opens its own output and verifies magic + frame count, because a
  structurally valid but undecodable GIF is the failure mode here.

When touching export, verify by **decoding**, not by parsing headers:

```sh
python3 -c "from PIL import Image; im=Image.open('/tmp/multi.gif'); print(im.n_frames, im.size, im.info.get('loop'))"
```

---

## 10. Install and menu integration

`install.sh` (idempotent, no sudo, nothing outside `$HOME`):

```
~/.local/bin/levele
~/.local/share/icons/hicolor/scalable/apps/levele.svg
~/.local/share/icons/hicolor/{16,24,32,48,64,128,256,512}x*/apps/levele.png
~/.local/share/applications/com.nodaysidle.levele.desktop
```

The Omarchy menu is a Quickshell layer that lists `DesktopEntries.applications` and
resolves icons by scanning `~/.local{,/share}/icons/**/apps/*.{svg,png}` (SVGs first).
So: a standard XDG desktop entry + a hicolor icon is all that is required — there is no
Omarchy-specific registry to update.

**Gotcha that will bite you:** `main.cpp` calls `setDesktopFileName("com.nodaysidle.levele")`,
which makes that the Wayland `app_id`, so the desktop entry's `StartupWMClass` must be
`com.nodaysidle.levele` — not `levele`. Otherwise the dock/taskbar cannot match the window
to the entry.

Launching is done by the shell as `uwsm-app -- gtk-launch com.nodaysidle.levele.desktop`.

---

## 11. Known gaps

- **Video export** — none. The web app used `MediaRecorder`; there is no Qt equivalent
  worth faking.
- **Not in the rewrite at all**: accounts, share links, MQTT live collaboration, comments,
  presentation mode.
- **Unverified interaction paths**: layer drag-reorder, marquee selection, and the
  file-picker leg of export were never exercised end-to-end (the session that built this
  had keyboard automation only, no pointer tool). The code is there; the buttons and the
  export functions are individually proven. If you touch those areas, drive them for real.
- **Text**: no italic/underline/uppercase/tracking UI, no vertical-align control in the
  inspector (the model has `textVAlign`, the UI does not expose it), no per-run styling.
- **Images**: only `cover`/`contain`, no crop tool, no rounded-corner mask UI beyond the
  shared radius field.
- **Shapes**: no blob/burst/heart from the web version, no icon library, no stickers.
- **Undo**: whole-document snapshots. Fine at this size; if documents get large, switch to
  command objects.
- `qmllint` cannot resolve the `Levele` QML module because `CanvasItem` is registered from
  the executable (`qmlRegisterType` in `main.cpp`). Everything it reports about
  `CanvasItem` / `CanvasStage::canvas` / `Loader.item` is that one limitation cascading.
  There are currently **no other qmllint findings** — keep it that way.

---

## 12. Traps that already cost time — do not repeat

1. **A QML property with the same name as a context property silently receives null.**
   `CanvasItem { studio: studio }` binds the item's *own* `studio` property to itself, the
   engine never sets it, and nothing is logged — the canvas just stays blank. The C++
   property is therefore called `document`. Do not rename it back.
2. **Never cache a formatted value and read it in a change handler.**
   `onValueChanged: text = display` can run before the `display` binding re-evaluates, so
   the widget keeps the stale value and looks frozen. `NumField` formats from
   `root.value` inside the handler for exactly this reason.
3. **A widget has no size at `Component.onCompleted`.** `CanvasStage` keeps calling
   `fitToView()` on width/height changes until the user pans or zooms (tracked by
   `CanvasItem::userMoved`), instead of fitting once and getting a 5 % zoom.
4. **`QQuickPaintedItem` needs a `document`/size before it paints anything** — a zero-sized
   item is silently blank.
5. **Text boxes must be sized for the wrapped text.** The renderer draws the full
   `QTextDocument` height (correct for an editor), so a box sized for one line makes a
   two-line headline overlap whatever is below it. Four templates had this bug.
6. **`--selftest` constructs a `Studio`, which autosaves.** It overrides `XDG_DATA_HOME` to
   a scratch dir for that reason. Do not remove that line.

---

## 13. Conventions

- C++17, Qt 6.12, `-Wall -Wextra` and **zero warnings** — the build is expected to stay
  clean (`grep -c 'warning:' build.log` → 0).
- No Qt Quick Controls styling shortcuts: `QQuickStyle::setStyle("Basic")` and every
  control is painted from `theme.*` in `qml/ui/`. Do not introduce a stock-styled control;
  wrap it.
- Fonts: **Noto Sans** for UI, **JetBrains Mono** for every numeric field. Do not reference
  Inter — it is not installed.
- Colours only ever come from `theme`. A hardcoded hex in QML chrome is a bug; hardcoded
  hexes inside *template content* are fine (that is user artwork).
- Element properties are the contract between the canvas, the inspector, the JSON
  round-trip and the renderer — adding one means touching `Element`, `toJson`,
  `fromJson`, `sel()` and `setSelProp()` together.
- New QML file → add it to `resources.qrc`.
- `--demo` opens template 0 straight in the editor. Handy for screenshots; it autosaves a
  document, so clean `~/.local/share/levele/docs` afterwards if you care.

---

## 14. Driving it for verification on this machine

```sh
export WAYLAND_DISPLAY=wayland-1
export HYPRLAND_INSTANCE_SIGNATURE=$(ls /run/user/$(id -u)/hypr/ | head -1)

# Hyprland 0.56 dispatch is Lua now: `hyprctl dispatch` takes Lua, not the old strings.
hyprctl eval 'hl.dispatch(hl.dsp.focus({ workspace = 4 }))'
ADDR=$(hyprctl clients -j | jq -r '.[] | select(.class|test("nodaysidle.levele")) | .address' | head -1)
hyprctl eval "hl.dispatch(hl.dsp.focus({ window = \"address:$ADDR\" }))"
hyprctl eval 'hl.dispatch(hl.dsp.window.float({ action = "toggle" }))'

grim -o DP-1 /tmp/shot.png                 # whole screen
grim -g "$X,$Y ${W}x${H}" /tmp/win.png     # just the window, from hyprctl clients
wtype -M ctrl -k a -m ctrl                 # keyboard driving; no pointer tool installed
```

Menu check (proves the entry is really listed, not just present on disk):

```sh
omarchy-menu toggle; sleep 1; wtype "levele"; grim -o DP-1 /tmp/menu.png; omarchy-menu close
```

Ignore the `Gtk-WARNING: gtk.css … is not a valid color name` lines at startup — they come
from the user's GTK theme file, not from this app.

---

## 15. Sensible next steps

1. Exercise the unproven interactions (drag-reorder, marquee, export file picker) and fix
   whatever they reveal.
2. Text: expose `textVAlign`, add italic/underline, per-element font choice from the
   installed families.
3. Image: a crop mode and a proper `contain`/`cover` visual affordance.
4. Rotated-element snapping and multi-element resize (currently resize acts on the primary
   selection only).
5. `git init` in this directory — it is currently untracked and unversioned (`.gitignore` is already in place).

---

## 16. Recent Polish & Parity Pass (2026-11-17)

- **All 15 templates ported** (`src/templates.cpp`): added `The quiet north`, `Form & function`, `30-day experiment`, `Amara & Noah`, `Thirty, flirty`, `Studio North` (2 pages), and `Elena Marsh`; updated `src/main.cpp` multi-page selftest to index `Fieldwork portfolio`.
- **`/omarchy-app` additions**: `Theme::radius` queried from `hyprctl -j getoption decoration:rounding`; `?` (`ShortcutsOverlay.qml`) and `Q` (save + quit) shortcuts in `Main.qml`; out-of-tree `bin/{build,test,install}` and `.gitignore`; `install.sh` prefers `build/levele`.
- **`/unslop` UI pass**: sentence-case section headers (removed tracked uppercase), 4px spacing/sizing grid, `>=11px` type across metadata/labels, and `theme.radius` across panels/dialogs/controls.
- **Verified**: `./bin/test` (`0` compiler warnings, `SELFTEST PASS (0 failing checks)`, `89 themes checked, 0 with failing pairs`), `./bin/install`, and `timeout 3 ~/.local/bin/levele` (`launch_exit=124`).
