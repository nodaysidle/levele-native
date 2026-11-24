# HANDOFF — levele-native

## Swarm code & UI/UX audit, fixes, and install — 2026-11-24 (pass 2)

Full C++ (`src/`), QML UI/UX (`qml/`), and regression-test (`tests/`) pass followed by reinstall and Omarchy Apps refresh:

### C++ core fixes (`src/`)
1. **Inspector & canvas selection sync** (`src/canvasitem.{h,cpp}`, `src/studio.cpp`): Emitted `selectionChanged()` on canvas gesture completion (`Mode::Creating`, `Moving`, `Resizing`, `Rotating`), `commitEditText()`, and `Studio::renameElement()`; confirmed `Studio::sel()` exposes `"locked"`.
2. **Signed-vector `line`/`arrow` geometry** (`src/canvasitem.cpp`): Preserved signed `(w, h)` and zero-axis dimensions in `Mode::Resizing` (`m_startLine`) and snapped via `Element::rect()` in `applySnap()`.
3. **Rotation, text editing & shortcuts** (`src/canvasitem.cpp`): Fixed `Mode::Rotating` 180° wraparound jumps and Shift 15°-snap stalls (`m_startRotation` + fixed `m_pressScreen`); added `Key_Return`/`Key_Enter` inline text edit activation on single unlocked `text` elements; cleared `m_editingId` + emitted `editingChanged()` on page/doc/target changes and blocked editing on locked layers; guarded shortcuts against `AltModifier`; kept `m_spaceDown` across multi-drag pans; guarded `nudge()` on locked-only selections; synced `updateEditRect()` in `resetView()`.
4. **Controller, undo & input guards** (`src/studio.cpp`, `src/document.cpp`): Tracked active `m_pageIndex` and cleared stale cross-page selection in `movePage()`; added `fi.isFile()` guards in `addImage()`/`replaceImage()` and locked guard in `replaceImage()`; aligned `setSelProp()` multi-select reference (`m_selection.first()`) and mixed-type deltas; clamped `newDocument()` dimensions `[16, 16384]`; reset `m_nudgeOpen` across `undo()`/`redo()`/`resetHistory()`; skipped no-op undo snapshots in z-order ops and validated `setPageBackground()`. Replaced empty `"id"` in `Element::fromJson()`/`Page::fromJson()`, clamped `fontWeight` in `double` before int conversion, and handled element rotation in `Element::intersectsRect()`.
5. **Exporter & theme lifecycle** (`src/exporter.cpp`, `src/theme.{h,cpp}`): Registered `qAddPostRoutine(clearTextDocCache)` + `!QCoreApplication::closingDown()` guard for `QTextDocument` cache; clamped `imageFor()` scaled size `>= 1`; scoped `QPdfWriter`/`QPainter` before `QSaveFile::commit()`; added `Theme::~Theme()` to clean up running `hyprctl` `QProcess` instances; checked path/XDG existence in `rewireWatcher()`; bounded and cleared `m_eventBuffer` on socket disconnect.

### QML UI/UX & accessibility fixes (`qml/`)
6. **QML runtime errors & binding loops** (`qml/CanvasStage.qml`, `qml/LayersPanel.qml`, `qml/Inspector.qml`): Added `applyingLineHeight` re-entrancy guard in `applyLineHeight()` (eliminating `RangeError` stack overflow); fixed `ReferenceError: root is not defined` on layer drag release and replaced `ListView.height` `contentHeight` binding loop with deterministic row-count height + `ScrollBar.vertical` (fixing bottom-aligned layers); guarded `studio.sel.type` against `undefined` on empty selection.
7. **Inspector capabilities & locks** (`qml/Inspector.qml`): Added dedicated `Stroke` section visible for `line`/`arrow` and shapes (`Fill` stays hidden for `line`/`arrow`), allowed signed/zero `W`/`H` (`min: -100000`) for `line`/`arrow`, added `"Locked"` badge + `"Unlock"` button while disabling mutable fields when locked, wired `mixed` state on `fontSize`, tightened Track/Weight row widths, and wrapped strings in `qsTr()`.
8. **Responsive layout, modals & focus rings** (`qml/Main.qml`, `qml/Editor.qml`, `qml/Dashboard.qml`, `qml/PageStrip.qml`, `qml/ExportDialog.qml`, `qml/ShortcutsOverlay.qml`, `qml/ui/*.qml`): Themed `leaveDialog` and capped `toast` width with `ElideMiddle`; gated global shortcuts on `!win.modalOpen` and moved `Escape` selection clear to `Editor.qml`; prevented `640x480` `topBar` overlap and added `Escape` revert + canvas refocus on `titleField`; anchored `Dashboard` Recent title, wrapped empty state, and elided template labels; unclipped `PageStrip` focus ring and fixed chip/number spacing; capped `ExportDialog` and `ShortcutsOverlay` to overlay bounds with dynamic row heights and focus commit before export; switched external focus rings (`Btn`, `IconBtn`, `AlignBtn`, `Toggle`) to `theme.accent`, fixed `ColorChip` popup `x` clamp + keyboard-focusable swatches, added `Escape` revert/blur on `NumField`, removed duplicate padding in `Combo`, and added `checkable`/`toggleable` accessibility properties.

### Verification & Install (2026-11-24 pass 2)
- **Builds** (`build/levele`, `build-tests/levele_tests`, `build-harness/harness` with `-Wall -Wextra`): **0 compiler warnings**.
- **`./bin/test`**: **PASS** (`SELFTEST PASS (0 failing checks)`, 15 templates, PNG/PDF/GIF; `--theme-sweep`: 83 themes checked, 0 failing, 1 skipped; 0 `hyprctl` `QProcess` warnings).
- **`tests/levele_tests`** (`QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= dbus-run-session -- ./build-tests/levele_tests`): **39 passed, 0 failed, 0 skipped** (includes 6 new C++ regression tests).
- **`tests/qml-harness/harness`** (`QT_QPA_PLATFORM=offscreen ./build-harness/harness`): **18/18 scenarios passed** (`ui/AlignBtn.qml` added to `check-qml`, plus `phase2-regressions`), **0 QML/Qt runtime warnings or errors**.
- **`python3 tests/contrast.py`**: **PASS** (83 themes checked, 12 light, 1 skipped `osaka-jade@usr`, **0 failing pairs**).
- **Reinstalled to `~/.local` & Omarchy Apps**: Ran `./bin/install` (`build/levele` and `~/.local/bin/levele` match identically via `cmp -s`, SHA-256 `7cfd2da1b8221935cbc3d4f4ceb8eba0ee1dfe883df744c6b30900b4d7bdb41f`), `desktop-file-validate ~/.local/share/applications/com.nodaysidle.levele.desktop` passed, `omarchy menu refresh` returned `ok`, and `apps.levele` in `~/.config/omarchy/extensions/omarchy-menu.jsonc` launches `uwsm-app -- gtk-launch com.nodaysidle.levele.desktop`.
- **Live GUI smoke launch (Hyprland workspace 3)**: Launched via `omarchy menu summon apps.levele`, verified mapped window `com.nodaysidle.levele` (`Levele`, `2550x1400`) on workspace 3 with `/proc/<pid>/exe` → `/home/arch/.local/bin/levele`, confirmed stable startup and clean close via `hl.dsp.window.close`, and restored workspace 2.

---

## Audit & fixes — 2026-11-24

Full codebase audit and remediation of all Critical, High, and Medium findings:

### Critical & High fixes
1. **Background export shutdown & text-cache safety** (`src/studio.{h,cpp}`, `src/exporter.cpp`): Tracked active export futures in `Studio::m_exportFuture` and awaited completion in `Studio::~Studio()`. Restricted `QTextDocument` caching to the GUI thread (cleared on `aboutToQuit`), using a scoped `std::unique_ptr<QTextDocument>` on worker threads so no `QObject` outlives `QGuiApplication`.
2. **Atomic PDF export** (`src/exporter.cpp`): Switched `Exporter::writePdf()` from direct `QPdfWriter(path)` to `QSaveFile` + `QPdfWriter(&file)` with `cancelWriting()` on failure and `file.commit()` on completion.
3. **Locked layer protection** (`src/studio.cpp`, `src/canvasitem.cpp`): Guarded `Studio::setSelProp()` and `CanvasItem` `Mode::Moving` (press + move) against mutating locked layers selected from `LayersPanel`.
4. **Hostile JSON & non-regular image file guards** (`src/document.cpp`, `src/exporter.cpp`): Added `std::isfinite()` validation and numeric clamping for all `Element::fromJson()` fields, validated `Page::background`, and required `fi.isFile()` + bounded `maxSide` in `Exporter::imageFor()` to prevent device-node/FIFO hangs (e.g. `/dev/zero`).
5. **Inline text editor state sync** (`src/canvasitem.cpp`): Emitted `editingChanged()` when clearing `m_editingId` in `CanvasItem::mousePressEvent()`.
6. **Export URL validation** (`src/studio.cpp`): Rejected non-local URL schemes in `exportPng`/`exportPdf`/`exportGif` via `localExportPath()`.
7. **Read-only interactions no longer dirty documents** (`src/canvasitem.cpp`, `src/studio.cpp`): Restricted `CanvasItem::mouseReleaseEvent()` `touch()` to actual moved gestures, and removed `scheduleSave()` from `Studio::setCurrentPage()`.

### Medium fixes
8. **Hyprland radius query race & socket buffer cap** (`src/theme.cpp`): Checked `m_radiusQuery == proc` before nulling in `finished`/`errorOccurred` handlers, and capped `m_eventBuffer` at 64 KiB.
9. **Signed-vector lines/arrows in Inspector, align, and distribute** (`src/studio.cpp`): Preserved signed `w`/`h` for `line`/`arrow` in `setSelProp()`, and aligned/distributed via `Element::rect()` deltas in `alignSelected()` and `distributeSelected()`.
10. **Single-pass GIF frame rendering** (`src/exporter.cpp`): Cached rendered frames during `buildPalette()` when total raw frame memory is `<= 64 MB`, avoiding double page rendering for normal GIF exports while keeping streaming fallback for huge exports.
11. **Recent document ID filtering** (`src/studio.cpp`): Filtered `Studio::listDocs()` through `Doc::isValidId()`.
12. **Sanitised asset file extensions** (`src/studio.cpp`): Added `safeImageExt()` to strip non-alphanumeric suffix characters in `addImage()` and `replaceImage()`.
13. **QML harness isolation & fixtures** (`tests/qml-harness/harness.cpp`): Isolated `XDG_DATA_HOME` into a `QTemporaryDir` by default (opt-out via `LEVELE_HARNESS_KEEP_DATA=1`) and generated temporary PNG fixtures in `f09-f10-f21` and `f11-f26`.

### Verification (2026-11-24)
- `./bin/test`: PASS (`SELFTEST PASS`, 83-theme sweep 0 failing).
- `tests/levele_tests`: 32 passed, 0 failed (including 6 new regression tests covering locked layers, hostile JSON + `/dev/zero`, atomic PDF, remote export URLs, background export teardown, signed-line align, and invalid doc ID filtering).
- `tests/qml-harness/harness`: 15/15 scenarios passed (`check-qml`, `f03-f13`, `f01-quit-save`, `f22-f25`, `f09-f10-f21`, `f16-f20`, `f11-f26`, `f12-f08-f07`, `a11y-shape`, `f19-targets`, `f06-tab-walk`, `trash-dashboard`, `mixed-inspector`, `portal-routing`, `f17-small-window`).
- `python3 tests/contrast.py`: PASS (83 themes, 0 failing pairs).

---

## Audit fixes and deployment status — 2026-11-20

This section records the previous build. The 2026-11-18 notes below document the earlier audit and implementation pass.

1. **Image chooser.** Open and Save portal requests now complete separately, with a Qt dialog fallback. Image placement and replacement check the original document, page, and target layer when the chooser returns. PNG export also checks the original page.
2. **Project identity.** A saved project's filename determines its identity. Opening a project leaves it clean, and subsequent edits save to the file the user opened.
3. **Failed saves.** Back, `Q`, and compositor close commit pending fields before saving. A failed write keeps the editor open and offers Retry save, Discard changes, and Cancel.
4. **Render concurrency.** Canvas and export threads use separate image and text-layout caches.
5. **Trash.** The dashboard can restore deleted projects across sessions. The seven-day retention window uses the recorded deletion time. Restoring a project whose filename is already in use preserves both designs. Older trash entries without a deletion timestamp remain recoverable.
6. **Layer locks.** Select All skips locked layers, and deletion protects locked layers even when they are selected explicitly.
7. **Compact layout.** The Inspector starts closed at compact widths, keeping the fitted page visible on entry.
8. **Mixed selections.** Numeric Inspector fields update their Mixed state as selection values change.
9. **Dashboard.** Recent projects appear before the template gallery.
10. **Export limits.** GIF export streams frames and caps each side at 8,192 pixels, each frame at 8 million pixels, and the complete export at 512 million pixels and 1,024 frames. GIF and PNG writes commit atomically.
11. **Desktop launcher.** The installer generates a desktop file that launches Levele directly. It drops the unsupported project MIME association and `%F` file arguments.

### Verification

- Qt 6.12 `qmake6` builds passed in and out of tree. The app self-test passed, the theme sweep passed across 83 themes, and all 16 offscreen QML harness invocations passed. Harness coverage includes chooser routing, mixed Inspector fields, keyboard and accessibility checks, and screen captures.
- Offscreen native window-close Retry and concurrent raster rendering probes passed. Pillow decoded the one-frame and three-frame GIF samples, and `pdfinfo` reported three pages for the multi-page PDF.
- All available non-socket Qt checks passed. The Hyprland reload test could not bind its local UNIX socket. The portal integration test was skipped because the sandbox also blocked the private D-Bus daemon's socket. The chooser-routing harness simulates portal completion.
- A staged install at `/tmp/levele-install-stage.wn8KyA` matched the checksum of `build/levele`, and `desktop-file-validate` accepted its launcher.

### Install and Omarchy Apps

The existing **Apps → Levele** entry is `apps.levele` in `~/.config/omarchy/extensions/omarchy-menu.jsonc` (`uwsm-app -- gtk-launch com.nodaysidle.levele.desktop`), reused without modification.

- **Installed to `~/.local`:** Ran `./bin/install` from `~/dev/nodaysidle/levele-native` (`bin/build` + `install.sh`) and `omarchy menu refresh` (`ok`). Both `build/levele` and `~/.local/bin/levele` match identically (`cmp -s` clean, SHA-256 `a46d188895bbd30c50d9e7258818df17baf35b80ff3d00b83e63c7d655d29baf`).
- **Desktop entry:** `desktop-file-validate ~/.local/share/applications/com.nodaysidle.levele.desktop` passed cleanly (`Exec=/home/arch/.local/bin/levele`, no `%F`, no `MimeType=`).
- **Live Omarchy launch & real picker checks:** Launched Levele from Omarchy Apps (`omarchy menu summon apps.levele`) on an isolated Hyprland workspace and confirmed `/proc/<pid>/exe` resolved to `/home/arch/.local/bin/levele`. Created a disposable project, imported a disposable `320×240` PNG via the live `flea` portal file picker (`Place an image`), confirmed the original image's bytes, size (`1358` bytes), mtime, and SHA-256 (`df53ab518ade3eebd71268ceddfafce43f7d647122813c9ff94fca160ea55cbb`) remained unchanged, then exported PNG (`2×`, `2160×2160`) to a separate directory via the live `flea` portal save picker (`Export to file`). Pillow verified the exported PNG (`28005` bytes, `RGBA`) and the saved project retained exactly one layer (`disposable-input`; no layer added on export).
- **Cleanup & data preservation:** Quit Levele cleanly with `Q`, restored the active workspace, and removed the disposable project, imported asset copy, and `/tmp` artifacts. Existing project data (`49031059-0dff.json`, `fd31600a-a6c5.jpg`, no `.trash`) and `omarchy-menu.jsonc` matched their pre-deployment SHA-256 checksums.
- **Remaining blockers / un-run items:** No deployment or live desktop blockers remain for this build. The earlier sandbox-skipped socket-based Qt unit tests (`tests/levele_tests` Hyprland reload and private D-Bus portal test) were not re-run separately in this pass (live Hyprland launch and live `flea` D-Bus open/save portals were exercised instead). Historical decisions/manual items from the 2026-11-18 record (PKGBUILD/`git init`, Orca speech walk-through, hardware touchpad feel) remain noted below.

---

## Historical record — 2026-11-18

Date: 2026-11-18 · Author: Audit-Hawk · Target: Arch Linux / Omarchy 4.5 (Hyprland 0.56.2, Wayland, `scrolling` layout, DP-1 2560x1440 @ 1.0)
Repo: `/home/arch/dev/nodaysidle/levele-native` (**not a git repo**; no commits; in-tree `build/` was refreshed with the 2026-11-18 build — see the implementation section below)
Previous handoff: renamed to `HANDOFF.2026-11-18.prev.md` (a build/architecture doc, not an audit; status of its claims below).

## Summary
1. Stack: C++17 + Qt Quick 6.12 (qmake6, Basic style, all controls custom-painted from `theme.*`), one binary, ~6.3k LOC. A layered design studio (shapes, text, images, pages, PNG/PDF/GIF export) ported from the web app `../levele` (Next.js 16 + Konva 10). No network, no LLM/API calls.
2. Health: out-of-tree build is clean (0 warnings with `-Wall -Wextra`), `--selftest` passes (15 templates, PNG/PDF/GIF; GIFs decode in PIL), `--theme-sweep` passes 90 themes. The core editor works, but data safety, accent-button contrast, line selection and keyboard/a11y are weak, and the sweep checks too few colour pairs to catch the contrast failures.
3. Top risks: F01 closing the window (SUPER+W) drops the last <800 ms of edits plus any uncommitted Inspector/title text; F02 white text on mid-tone accents fails WCAG on 40/90 themes, including Alan's current `nodaysidle-gallery` (every accent button is 2.86:1); F03 lines and arrows with a zero width/height (the default line, and 10 "Rule" lines in templates) can't be clicked or marquee-selected on the canvas.

---

# Implementation pass — 2026-11-18 (Boris) — READ THIS FIRST

The audit above is the record of *what was wrong*. This section is the record of *what was done*. All findings are implemented and verified except **F27** and **F32's PKGBUILD/`git init`**, which still need Alan's decision. The app is built, tested, installed and in the Omarchy menu.

## Status per finding

| ID | Status | Where the fix lives |
|---|---|---|
| F01 | done | `src/main.cpp:405` (`QQuickWindow::closing` → `Studio::closingSoon()` → save), `qml/Main.qml` (commit pending text), `Studio::saveOnQuit` |
| F02 F18 F31 | done | `Theme::contrastPairs()`, `Theme::bestInkOn()`, `readable()` over 5 panels, border 3.0; `runThemeSweep()` reads the same table |
| F03 F13 | done | `Element::containsPoint/intersectsRect` (`src/document.cpp`), `CanvasItem::hitTest`, drawn line vectors in `mouseReleaseEvent` |
| F04 | done | `Studio::deleteDoc/restoreDoc/undoDelete/purgeTrash`, toast Undo in `qml/Main.qml` |
| F05 | done | `Studio::writeDoc` (QSaveFile), `saveWithFeedback`, read-only-dir status |
| F06 F19 | done | `qml/ui/IconBtn.qml`, `qml/ui/AlignBtn.qml`, `Accessible.*` in 17 files, focus rings, 24 px targets |
| F07 | done | `Studio::documentReplaced()`, `qml/CanvasStage.qml` fits only on that |
| F08 | done | `takeGestureSnapshot/commitGestureSnapshot/discardGestureSnapshot`, `beginNudge()` (500 ms), `touchSelectionOnly()` |
| F09 | done | `Studio::geometryChanged/touchGeometry`, `primaryId()/primaryElement()`, cached text layout in `src/exporter.cpp` |
| F10 | done | `QtConcurrent::run` + `QFutureWatcher` in `Studio::exportPng/Pdf/Gif`, `exporting` property, `QT += concurrent` |
| F11 | done | `Studio::addElementAt`, `setPointerPosition`, images land under the cursor |
| F12 | done | `CanvasItem::keyReleaseEvent` + `focusOutEvent` |
| F14 | done | `Studio::replaceImage` |
| F15 | done | `setSelProp` deltas for multi-select, `Studio::selMixed`, "Mixed" placeholder in `qml/ui/NumField.qml` |
| F16 | done | horizontal `ListView` page strip, drag reorder, keyboard paging/reorder, 24 px delete, explicit duplicate button |
| F17 | done | min size 640×480, `Ctrl+1/Ctrl+2` + `[`/`]`, compact inspector overlay under 1100 px |
| F20 | done | `preventStealing: true` + Alt+↑/↓ in `qml/LayersPanel.qml` |
| F21 | done | `QImageReader` in `addImage`, bounded image cache + "Missing image" placeholder in `src/exporter.cpp` |
| F22 | done | `CanvasItem::zoomFactorForDelta` (`pow(1.0015, delta)`), `pixelDelta()` pan |
| F23 F33 | done | `function(mouse/event)` handlers; `themeColoursChanged`; `std::as_const`; function-static caches |
| F24 | done | `Doc::isValidId/sanitiseId`, size clamp 16..16384, file basename is the id |
| F25 | done | `editWeight/editAlign/editVAlign/editSpacing/editLineHeight`; line height applied through `QQuickTextDocument` |
| F26 | done | per-type Inspector sections, `AlignBtn` (left/centre/right/justify), vertical alignment, image radius |
| F27 | **blocked** | unchanged — Alan decides drop `%F`/`MimeType` vs implement file open |
| F28 | done | `src/portalfilechooser.{h,cpp}` (portal FileChooser over D-Bus, QtQuick.Dialogs fallback), `Studio::suggestedExportName`, hex + swatches in `qml/ui/ColorChip.qml` |
| F29 | done | `Theme::queryRadius()` (async QProcess), `Theme::watchHyprlandEvents()` (`.socket2.sock`, `configreloaded`) |
| F30 | done | 156 `qsTr()` in QML, 35 `tr()` status strings, `QTranslator` hook for `translations/levele_<lang>.qm` |
| F32 | **tests done, packaging blocked** | `tests/levele_tests.{pro,cpp}` (22 tests); `INSTALLS` dropped from `levele.pro`; PKGBUILD/`git init` still Alan's call |
| F34 F35 | done | "Start from a size", recent-row thumbnails, `Studio::docJson`, rewritten `qml/ShortcutsOverlay.qml` (25 rows), `U/P/Y/A/I` tool keys |

## Verification gates — current results

```bash
R=/home/arch/dev/nodaysidle/levele-native; B=/tmp/levele-native-build
mkdir -p $B && cd $B && qmake6 $R/levele.pro && bear --output $B/compile_commands.json -- make -j$(nproc) 2>&1 | tee $B/build.log
grep -c 'warning:' $B/build.log                 # 0
QT_QPA_PLATFORM=offscreen $B/levele --selftest /tmp/levele-selftest   # PASS, exit 0
QT_QPA_PLATFORM=offscreen $B/levele --theme-sweep                     # 90 themes, 0 failing, skipped 1, exit 0
python3 tests/contrast.py                     # 0 failing pairs (mirror of Theme::loadFrom; checks the same 12 pairs)
qmllint -I qml -I qml/ui qml/*.qml qml/ui/*.qml | grep -c 'is ambiguous'   # 0
CLAZY_HEADER_FILTER='.*/levele-native/src/.*' clazy-standalone ...  # 0 in src/
cppcheck --library=qt --enable=warning,performance --std=c++17 -I src src  # 0
shellcheck -s sh install.sh bin/*               # only the intentional SC1007 `CDPATH= cd` idiom
```

`--selftest` now also asserts: save feedback, read-only dir failure, trash/restore, line picking (element + 10 template lines), hostile document clamping, and the zoom factor.

### Qt Test suite (22 tests)

```bash
mkdir -p /tmp/levele-tests-build && cd /tmp/levele-tests-build
qmake6 $R/tests/levele_tests.pro && make -j$(nproc)
QT_QPA_PLATFORMTHEME= QT_QPA_PLATFORM=offscreen dbus-run-session -- ./levele_tests
# Totals: 22 passed, 0 failed
```

Covers F03, F04, F05, F08, F24, F02/F18 (all 90 themes), F22, **F29** (fake `hyprctl` on PATH + a `QLocalServer` as `.socket2.sock`: radius 14 → `configreloaded` → 20), **theme live reload** (rewrites `colors.toml` in a sandbox `XDG_STATE_HOME`), and **F28** (a fake `org.freedesktop.portal.Desktop` on a private bus returns a URI; asserts the client accepts it and pre-fills the name).

`QT_QPA_PLATFORMTHEME=` matters: with the GTK platform theme set, the theme probes the portal at startup and takes the bus name before the fake can register (the test then skips rather than fails).

### Offscreen QML harness (11 scenarios)

Source lives in `tests/qml-harness/` (built and run in /tmp only):

```bash
mkdir -p /tmp/qml-harness-build && cd /tmp/qml-harness-build
qmake6 $R/tests/qml-harness/harness.pro && make -j$(nproc)
for s in f01-quit-save f03-f13 f06-tab-walk f11-f26 f12-f08-f07 f16-f20 \
         f19-targets f17-small-window f09-f10-f21 f22-f25 a11y-tree check-qml; do
  XDG_DATA_HOME=/tmp/levele-harness-data QT_QPA_PLATFORM=offscreen ./harness $s
done
```

Real mouse/key/wheel events, real QML, `/tmp` data only. Highlights: close 50 ms after an edit still saves; a zero-height line is hit-tested and marquee-selected; `w=300 h=2` survives the Line tool (and Shift snaps it flat); 200 drag moves emit `selectionChanged` **0** times and repaint each thumbnail **once**; 15 pages scroll in a 660 px strip and drag-reorder; 40 layers reorder; at 800×600 the canvas is 560 px; a 15-unit wheel delta zooms ×1.0227; the inline editor's `contentHeight` doubles at lineHeight 2.0; the accessibility tree has **79 named objects** in the editor and **27** on the dashboard with zero unnamed controls.

## Build / install / menu (how it is shipped now)

* `install.sh` prefers `build/levele` over `levele` — that in-repo binary was stale, so it was refreshed with the out-of-tree build before installing. **Refresh `build/levele` before re-running `install.sh`**, or the old binary is what ships.
* Installed: `~/.local/bin/levele` (sha256 matches the built binary), hicolor SVG + PNG icons, `~/.local/share/applications/com.nodaysidle.levele.desktop` (`desktop-file-validate` clean).
* Omarchy menu: `apps.levele` added to `~/.config/omarchy/extensions/omarchy-menu.jsonc` (`uwsm-app -- gtk-launch com.nodaysidle.levele.desktop`, aliases `levele, design, studio, canvas, poster`), then `omarchy menu refresh`. A backup of the JSONC from before the edit is next to it (`.bak.before-levele.*`).
* The installed binary was launched for 6 s offscreen from its own location: no QML errors (this is the check that the `resources.qrc` bundle is complete, which the file-URL harness cannot prove).

## Still open — needs Alan

1. **F27**: drop `%F`/`MimeType` from the desktop entry, or implement `levele open <file>` + a shared-mime-info XML. The entry still advertises `%F` and the MIME type exactly as before.
2. **F32 packaging**: PKGBUILD (`bin/install` = `makepkg -fsi`) and `git init`. Neither was done.
3. **Orca walk-through**: the AT-SPI bridge is verified up to the bus — the app registers as an application/frame and publishes 30 named objects (dashboard) with correct roles; the control subtree enumerates fully once the bus is healthy. Speech output itself is still a human check.
4. **Environment bug found and repaired**: `/run/user/1000/at-spi/bus_0` was refusing connections (ECONNREFUSED) while `at-spi-dbus-bus.service` reported *active* — Orca would have been dead for every app, not just this one. `systemctl --user restart at-spi-dbus-bus.service` fixed it. If assistive tech ever seems dead again, check that socket first.
5. Touchpad feel and the third-width tile are verified with synthetic events (×1.0227 per 15-unit delta, 560 px canvas at 800×600) but not by hand.

## Gotchas for the next agent

* **QML's `closing` handler never fires** on Qt 6.12: `QQuickCloseEvent` is only forward-declared in the public header, so the signal is not QML-visible. The close hook lives in `main.cpp` (`QQuickWindow::closing`) and QML connects to `Studio::closingSoon`.
* **A `Q_PROPERTY` WRITE accessor is not callable from QML.** `studio.setTitle(...)` silently did nothing until the method was marked `Q_INVOKABLE` — the title field never committed at all.
* **`TextEdit` has no `lineHeight`.** Use `textDocument` + `QTextBlockFormat::ProportionalHeight` (see `CanvasItem::applyEditLineHeight`).
* **`QQuickItem` children are not `QObject` children.** `findChildren` misses most of the tree; walk `childItems()`.
* **`console.log` in QML is filtered** unless `QT_LOGGING_RULES='qml.debug=true'` — several "silent" QML failures were just invisible logs.
* **clazy reports Qt's own headers** because qmake passes `-I`, not `-isystem`. `CLAZY_HEADER_FILTER='.*/levele-native/src/.*'` gives the real number; without it you get ~110 warnings from `/usr/include/qt6`.
* **New QML files need two things**: `import "ui"` in every file that uses them, and a `<file>` entry in `resources.qrc` (the file-URL harness passes without the qrc entry; only the installed binary notices).
* **Fake-portal tests** need `QDBusAbstractAdaptor` (plain `QObject` signals are not relayed on D-Bus), and the test must run with `QT_QPA_PLATFORMTHEME=` on a private bus.
* **`--selftest` writes only under the directory it is given**; the GUI writes to `$XDG_DATA_HOME/levele`. Point it at /tmp when testing the real binary, otherwise it touches Alan's two real designs.

## Files touched in this pass

`src/{canvasitem,document,exporter,main,studio,theme,templates}.{cpp,h}`, new `src/portalfilechooser.{cpp,h}`, `qml/{Main,Dashboard,Editor,CanvasStage,LayersPanel,Inspector,PageStrip,ExportDialog,ShortcutsOverlay}.qml`, `qml/ui/*.qml` (+ new `IconBtn.qml`, `AlignBtn.qml`), `resources.qrc`, `levele.pro`, new `tests/levele_tests.{pro,cpp}`, new `tests/qml-harness/{harness.pro,harness.cpp}`.

## Stack Detected
| Part | Detail | Evidence |
|---|---|---|
| Language / build | C++17, qmake6 (`QT += core gui qml quick quickcontrols2 svg`), `-Wall -Wextra`; Qt 6.12.0 (qt6-base 6.12.0-3, qt6-declarative 6.12.0-2, qt6-svg, qt6-imageformats, qt6-wayland 6.12.0) | `levele.pro:1-33`, `qmake6 -query QT_VERSION` |
| Entry / modes | `main.cpp`: `--selftest <dir>` (renders + exports + re-reads, sets `XDG_DATA_HOME` to the dir), `--theme-sweep` (loads every theme), `--demo` (opens template 0, autosaves into real data), GUI otherwise. File arguments are ignored. | `src/main.cpp:237-291` |
| C++ objects | `Theme` (context prop `theme`, parses `colors.toml`, `QFileSystemWatcher` on `current/` + debounce 150 ms, `hyprctl -j getoption decoration:rounding`), `Studio` (doc, selection, undo 100 snapshots, autosave 800 ms, doc store, export), `ElementListModel`, `CanvasItem` (`QQuickPaintedItem`, registered from the executable as `Levele 1.0`) | `src/theme.cpp:105-133`, `src/studio.cpp:117-127`, `src/main.cpp:277-285` |
| Renderer | One painter path `Exporter::paintPage` for canvas, thumbnails and exports; hand-written GIF89a encoder (median-cut + LZW) | `src/exporter.cpp:525-546,597-693` |
| QML | `Main.qml` (window, toast, Shortcuts), `Dashboard.qml`, `Editor.qml`, `CanvasStage.qml`, `LayersPanel.qml`, `Inspector.qml`, `PageStrip.qml`, `ExportDialog.qml`, `ShortcutsOverlay.qml`, `ui/{Btn,Caption,ColorChip,Combo,NumField,Section,Toggle}.qml`; all in `resources.qrc` | `resources.qrc:1-21` |
| Data | `~/.local/share/levele/docs/<id>.json` (plain JSON, 2 docs on this box), imported images copied to `…/assets/` | `src/studio.cpp:22-28,273-275,343-353,535-539` |
| Install | `install.sh` → `~/.local/bin/levele`, hicolor icons, `~/.local/share/applications/com.nodaysidle.levele.desktop` (`Exec=… %F`, `MimeType=application/x-levele-project`, `StartupWMClass=com.nodaysidle.levele`); installed copy dated 2026-11-17 20:01. No PKGBUILD. | `install.sh:20-59` |
| Tests | none besides `--selftest` / `--theme-sweep` (`bin/test` builds in-tree then runs them) | `bin/test:1-8` |
| Fonts | Noto Sans (UI), JetBrains Mono (numbers); both resolve via fontconfig here | `fc-match` |
| Sibling `../levele` | Next.js 16 + React 19 + Konva 10 web app (Yjs/MQTT collab, IndexedDB, accounts, animation, Present, WebM/MP4). Native is a deliberate subset port; see "Relation to ../levele". | `../levele/README.md` |

### Omarchy / Hyprland checks
| Check | Result | Evidence |
|---|---|---|
| Theme tokens across themes | 90 themes read (24 in `/usr/share/omarchy/themes`, 67 in `~/.config/omarchy/themes`, + current; 13 light). `osaka-jade` ships no `colors.toml` and is skipped. `quattro-glass` has no `mode` key and is treated as dark. Alias lists cover both `*_background` and `*_bg` key sets; `selection`/`selection_foreground` are ignored. | `theme-keys.txt`, `src/theme.cpp:246-286` |
| Light themes | detected only via `mode = "light"` in `colors.toml` (all 13 light themes have it); no `light.mode` files exist in Omarchy 4.x themes | `bgmode.txt` |
| WCAG contrast | App's own sweep: 0 failing (it checks only fg/textStrong/muted/red/green/yellow/border vs surface+chrome). Audit script (Appendix A, re-implements `Theme::loadFrom`, values match the app's sweep output for `current`): failures in pairs the QML actually paints — F02, F18. | `contrast.log`, `sweep.log` |
| Live reload | Watches the `current/` dir + `theme.name` + `colors.toml`, re-arms after each change (matches `omarchy-theme-set`, which rewrites `~/.local/state/omarchy/current/theme`). Static only; not exercised live. | `src/theme.cpp:187-212`, `/usr/share/omarchy/bin/omarchy-theme-set:12-21` |
| socket2 vs polling | No polling timers. `decoration:rounding` is read by a blocking `hyprctl` call on theme change only, never on Hyprland config reload (F29). | `src/theme.cpp:105-115,214-219` |
| HiDPI / fractional | `HighDpiScaleFactorRoundingPolicy::PassThrough` set; all fonts are `font.pixelSize` (ignore user font scale); canvas is a `QQuickPaintedItem` (DPR-aware). Alan's monitor is 1.0, so 1.25/1.5/1.75 are untested. | `src/main.cpp:239-240` |
| Keyboard-first | `?` overlay, `Q` save+quit, Ctrl shortcuts, tool letters only when the canvas has focus. Dashboard is mouse-only; most editor controls are `MouseArea`s with no focus (F06). | `qml/Main.qml:57-121` |
| Tiling sizing | `minimumWidth 1024`, `minimumHeight 680`, fixed side panels 240 + 288 px (F17). Alan uses the `scrolling` layout with `gaps_out 16`. | `qml/Main.qml:9-10`, `qml/Editor.qml:132,215` |
| CSD/SSD | `ApplicationWindow` with no custom decorations; Hyprland draws borders, no titlebar. Nothing to fix. | `qml/Main.qml:5-13` |
| Portals / dialogs | `QtQuick.Dialogs` `FileDialog`/`ColorDialog`; with Omarchy's `QT_QPA_PLATFORMTHEME=gtk3` these are GTK dialogs, not the portal FileChooser the Omarchy app convention asks for (F28). | `/usr/share/omarchy/default/hypr/envs.lua:13-14` |
| .desktop | `desktop-file-validate` clean; `StartupWMClass` matches `setDesktopFileName`. `%F` and `MimeType` are advertised but unimplemented (F27). | `install.sh:37-51`, `src/main.cpp:246` |
| Omarchy app conventions (`/usr/share/omarchy/default/agents/skills/omarchy-app/SKILL.md`) | Met: qmake6 single binary, theme live, `theme.radius`, `?` overlay, `Q` quits, offscreen self-test. Not met: atomic save (F05), insert under pointer (F11), portal dialogs (F28), `qsTr` i18n (F30), Qt Test + PKGBUILD + git (F32). | — |

### AI / API integration map
| Item | Value | Where |
|---|---|---|
| LLM / HTTP APIs | **None.** No `QNetworkAccessManager`, no URLs, no keys, no model ids. Env vars read: `XDG_DATA_HOME`, `XDG_STATE_HOME`, `XDG_CONFIG_HOME`, `HYPRLAND_INSTANCE_SIGNATURE` only. `rg -n -i 'anthropic\|claude\|openai\|haiku\|api[_-]?key\|QNetwork\|https?://\|typesafe' src qml levele.pro install.sh bin` → no hits. | `src/studio.cpp:24`, `src/theme.cpp:107,138,142` |
| Anthropic | No Anthropic code in this repo. (The sibling `../levele/AGENTS.md` is written for Claude Sonnet/Haiku/Opus delegation and there is a `../levele/.claude/` dir; outside this audit.) | — |
| TypeSafe (Jev) | Not used, not needed. | — |
| External process | `hyprctl -j getoption decoration:rounding`, 300 ms timeout, failure → radius 0 silently | `src/theme.cpp:105-115` |

### Relation to ../levele
Native is a port of the web app's element model, snapping, inspector fields, 15 templates and PNG/PDF/GIF export. Parity gaps (by design or not yet done): 12 typefaces → Noto Sans only; 11 shapes + 113 icons + 41 stickers → 7 shapes; italic/underline/uppercase/vertical align/highlight; grouping; pattern/image backgrounds; photo overlays and upload downscaling; page rename/reorder (Studio has `movePage`, no UI); animation, Present, JPG and video export; accounts/collab/comments; dashboard thumbnails and save-status indicator. Only native is audited here.

## Analyzers Run / Missing
Output: `/tmp/audit-levele-native-2026-11-18/`
| Analyzer | Result | File |
|---|---|---|
| `qmake6 …/levele.pro && bear -- make` in `/tmp/audit-levele-native-2026-11-18/build` | OK, 0 warnings; `compile_commands.json` (11 TUs) | `build.log`, `compile_commands.json` |
| `--selftest` (offscreen, out + `XDG_DATA_HOME` in /tmp) | PASS, 0 failing checks, 1.1 s | `selftest.log`, `selftest/` |
| PIL decode of `deck.gif`, `multi.gif`, `probe/big.gif` | all frames decode, `loop=0` | — |
| `--theme-sweep` | 90 checked, 0 failing (limited pair set) | `sweep.log` |
| Contrast script (Appendix A) | 10 painted pairs × 90 themes; failures in F02, F18 | `contrast.py`, `contrast.log` |
| Probe (scratch binary linking `document/templates/exporter.cpp`) | default line rect 300x0 → `contains()` false (F03); 10 template lines with a zero dimension; PNG 4x 935 ms, 3-page GIF @2000 px 456 ms on the GUI thread (F10); `Doc::fromJson` accepts width −5, height 1e9, id `../../evil` (F24) | `probe/probe.cpp`, `probe/probe.log` |
| `qmllint -I qml -I qml/ui` | 433 `[unqualified]` (context props `theme`/`studio`, noise), 9 `[missing-property]` + 4 `[unresolved-type]` + 5 `[import]` (all `CanvasItem`/`Loader.item` cascade, noise), **real:** `"mouse" is ambiguous` signal-parameter injection in `LayersPanel.qml:82-90` (F23), 2 unused imports | `qmllint.log` |
| `run-clang-tidy` bugprone/clang-analyzer/performance/concurrency | ~40 warnings, mostly `signed-bitwise` in the GIF encoder and `enum-size` (noise); float loop counter in grid painting; `EnumCastOutOfRange` on `QFont::Weight(1..1000)` (false positive) | `clang-tidy.log` |
| `clazy-standalone level1` | 114: `range-loop-detach` on non-const `elements()` in paint/hit-test paths, `overridden-signal` `CanvasItem::paletteChanged`, `non-pod-global-static` image cache (F33) | `clazy.log` |
| `cppcheck --library=qt` | `returnByReference` (7), `uninitMemberVarNoCtor` (`DocPreset`), one false-positive null check at `canvasitem.cpp:221` | `cppcheck-qt.log` |
| `g++ -fsyntax-only -Wall -Wextra -Wshadow -Wconversion` | 18 `-Wconversion`, 3 `-Wshadow` (hygiene) | `gxx-extra.log` |
| `shellcheck -s sh install.sh bin/*` | only SC1007 on the intentional `CDPATH= cd` idiom | `shellcheck.log` |
| `desktop-file-validate` (installed entry) | clean | — |
| **Missing / not done** | No PKGBUILD, so `namcap` N/A. No Qt Test suite. App GUI **not run** (rules + `--demo` writes real data), so Orca/AT-SPI, live theme switch, tiling at small sizes, touchpad zoom and layer drag-reorder are static findings marked UNVERIFIED where it matters. `QSG_RENDER_TIMING` not measured. | |

## Prior Handoff Status
Source: `HANDOFF.2026-11-18.prev.md` (written 2026-11-17 by the session that built the app). 16 claims/items checked: **4 hold, 4 stale/wrong, 6 still open, 1 partly done, 1 obsolete.**
| Prior item | Status | Evidence |
|---|---|---|
| §16 all 15 templates ported | holds | selftest `templates=15` |
| §16 `theme.radius` from `hyprctl` | holds (returns 14 here) | `src/theme.cpp:105-115` |
| §13 zero compiler warnings | holds | `build.log` 0 warnings |
| §6 theme sweep 0 failing pairs | holds, but checks too few pairs | F02, F18, F31 |
| §11 "no other qmllint findings" | stale: `mouse` injection | F23 |
| §6 "borders 3:1" | wrong: code enforces 2.6, sweep 2.5 | `src/theme.cpp:275`, `src/main.cpp:73`, F18 |
| §2 pages "reorder" | wrong: `movePage` has no UI | `src/studio.cpp:838-847`, F16 |
| §5 file map ("8 templates", templates.cpp 297 lines, Main.qml 104) | stale (15 templates, 521 / 122 lines) | `wc -l` |
| §11/§15.1 drag-reorder, marquee, export picker never exercised | still open; drag-reorder likely broken once the list scrolls | F20 |
| §11/§15.2 italic/underline/vertical-align UI | partly: "Track" (letter spacing) UI exists now; italic/underline/vAlign still missing | `qml/Inspector.qml:182-183`, F26 |
| §11/§15.3 image crop | still open (idea, not a task) | — |
| §15.4 rotated snapping, multi-element resize | still open (idea, not a task) | `src/canvasitem.cpp:418-475` |
| §15.5 `git init` | still open, Alan decides | F32 |
| §11 missing shapes/icons/stickers | still open (parity) | Relation section |
| §11 whole-document snapshot undo | still open, acceptable at this size | `src/studio.cpp:221-229` |
| §3/§11 video export, collab, Present not ported | obsolete (deliberate scope) | — |

## Findings
Counts: **0 P0, 6 P1, 16 P2, 13 P3** (35). Line numbers are 1-based in the current tree.

| ID | Sev | Area | file:line | Problem | Fix | Acceptance check |
|---|---|---|---|---|---|---|
| F01 | P1 | code/data | `src/main.cpp:260-291`, `src/studio.cpp:122-124`, `qml/Main.qml:5-13`, `qml/Inspector.qml:161`, `qml/Editor.qml:65` | Autosave is an 800 ms single-shot timer. Closing the window (SUPER+W, the normal Omarchy close) quits without `saveNow()`: there is no `aboutToQuit` hook, no `~Studio`, no `onClosing`. Edits made in the last 800 ms are lost, and so is text typed in the Inspector content field or the title field, which only commit on focus loss. Only the `Q` shortcut saves first. | Connect `QCoreApplication::aboutToQuit` → `studio.saveNow()` (save when the timer is active or dirty). In `Main.qml` `onClosing`, move focus off text fields first (`win.contentItem.forceActiveFocus()`) so pending commits land, then save. | `rg -n 'aboutToQuit' src/main.cpp` and `rg -n 'onClosing' qml/Main.qml` both hit; offscreen harness: edit, close window within 100 ms, reopen JSON → edit present |
| F02 | P1 | a11y/ui | `src/theme.cpp:278`; used in `qml/ui/Btn.qml:20`, `qml/Editor.qml:166`, `qml/Inspector.qml:221`, `qml/ui/Combo.qml:44`, `qml/ui/Toggle.qml:37`, `qml/CanvasStage.qml:54` | `accentInk` is black only when accent luminance > 0.42, so mid-tone accents (luminance ~0.18–0.42) get white text. 40/90 themes fail 4.5:1, worst 2.33 (afterburn), 2.37 (ristretto), 2.48 (gruvbox). Alan's current `nodaysidle-gallery`: white on `#d48473` = **2.86:1** (black = 7.35:1) on "New design", "Export", the active tool, align and combo highlight. `--theme-sweep` never tests this pair. | Pick the ink with the higher contrast: `contrast(black, accent) >= contrast(white, accent) ? black : white`. Add `{accentInk vs accent, 4.5}` to the sweep pairs (`src/main.cpp:66-74`). | `QT_QPA_PLATFORM=offscreen $B/levele --theme-sweep` asserts `accentInk` vs `accent` ≥ 4.5 and exits 0 (90 themes); `contrast.py` with `derive()` updated to the new rule → `accentInk/accent … 0 failing` |
| F03 | P1 | code/ux | `src/document.h:51-55`, `src/canvasitem.cpp:350-369,734-736,862`, `src/templates.cpp:35-46` | `Element::rect()` for a horizontal/vertical line or arrow has 0 height/width; `QRectF::contains()` and `intersects()` are always false for it, so these elements can't be clicked, hovered (no move cursor) or marquee-selected. Affects every line drawn with the default (300x0) and 10 "Rule" lines in templates (probe). Only the layers panel can select them. | Hit-test line/arrow by distance from point to segment ≤ `max(strokeWidth/2, 6/zoom)`; for marquee, test segment–rect intersection (or use `rect().adjusted(-t,-t,t,t)`). | Unit/probe test: `hitTest` at the midpoint of a 300x0 line returns its id; marquee over a template "Rule" selects it |
| F04 | P1 | ux/data | `qml/Dashboard.qml:267`, `src/studio.cpp:380-383` | "Delete" on a recent document removes the JSON file immediately: no confirmation, no undo, no trash, no toast. One mis-click next to "Copy" loses a design. | Move to `docs/.trash/<id>.json` and show a toast with "Undo" (restore), or confirm in a themed `Dialog`. Purge trash after N days. Exclude `.trash` from `listDocs`. | `rg -n 'trash' src/studio.cpp qml/Dashboard.qml` hits; selftest: `deleteDoc(id)` → file in `.trash`, `listDocs()` no longer lists it, restore brings it back |
| F05 | P1 | code/data | `src/studio.cpp:343-353,393-396` | `saveNow()` truncates and rewrites the document in place with `QFile`; a crash, power loss or full disk mid-write leaves a truncated/empty JSON, which `listDocs()` then silently skips (doc vanishes). Open/write errors are ignored and the user is never told; Ctrl+S gives no feedback either. Omarchy convention: "Save atomically". | Use `QSaveFile` (write, `commit()`), check the result, `setStatus("Couldn't save: " + errorString())` on failure, and a short "Saved" toast on Ctrl+S. Same for `duplicateDoc`. | `rg -n 'QSaveFile' src/studio.cpp` ≥2 hits; selftest with a read-only docs dir prints the failure status and exits 1 |
| F06 | P1 | a11y/keyboard | `qml/Editor.qml:11-20,150-184`, `qml/Dashboard.qml:108-114,195-201`, `qml/LayersPanel.qml:161-178,213-225`, `qml/PageStrip.qml:41-46,65-69,98-101`, `qml/Inspector.qml:225-229,286-292`, `qml/ui/ColorChip.qml:25-31`, `qml/ui/Section.qml:35-39`, `qml/CanvasStage.qml:106,120`, `qml/ui/Btn.qml:29-42` | `rg -c Accessible qml` → 0. Tool grid, preset chips, template cards, layer eye/lock, page chips, page delete, align buttons, colour chips, section headers and zoom ± are bare `MouseArea`s: not Tab-reachable, no role/name, so the Dashboard can't open a template without a mouse. `Btn` shows no focus ring. Glyph-only buttons (`←`, `↶`, `↷`, tools) have no name and the tool `tip` strings are never shown. | Use `AbstractButton`/`Btn` (or add `activeFocusOnTab`, `Keys.onReturnPressed/onSpacePressed`, `Accessible.role: Accessible.Button`, `Accessible.name`) for each; give `Btn` a `visualFocus` ring in `theme.accent`; add `ToolTip` from `tip`; `Accessible.name` on glyph buttons ("Back", "Undo", "Redo"). | `rg -c 'Accessible.name' qml` ≥ 30; `rg -n 'MouseArea' qml/Dashboard.qml qml/Editor.qml` only non-interactive hover areas remain; Tab from window start reaches "New design", presets, templates, recent "Open"; Orca live check by Alan |
| F07 | P2 | ux | `qml/CanvasStage.qml:31-34`, `src/studio.cpp:876,898-901` | `restore()` (undo/redo) emits `docChanged`, and `CanvasStage` calls `fitToView()` on every `docChanged`, so each Ctrl+Z throws away the user's zoom and pan. | Emit a separate `documentReplaced` from `newDocument/newFromTemplate/openDoc` and fit only on that. | `rg -n 'documentReplaced' src/studio.h qml/CanvasStage.qml` hits; harness: zoom 300 %, undo → zoom still 300 % |
| F08 | P2 | code/ux | `src/canvasitem.cpp:521,572,929`, `src/studio.cpp:238-249,704-716` | A plain click on an element (or a handle) pushes an undo snapshot even when nothing moves; each arrow-key nudge pushes one; `alignSelected` pushes before its early return. Ten clicks = ten no-op undo steps out of 100, and `touchSelection()` schedules an autosave on pure selection changes. | Snapshot into `m_pending` on press and push it only if `m_moved` on release; coalesce nudges (e.g. 500 ms window); push after the early return; don't `scheduleSave()` for selection-only changes. | Selftest: select an element 10 times → `canUndo == false`; 10 nudges within 500 ms → 1 undo step |
| F09 | P2 | perf | `src/studio.cpp:238-244`, `src/canvasitem.cpp:55-62,279,287`, `qml/Inspector.qml:12-16`, `qml/PageStrip.qml:32-39`, `src/exporter.cpp:86-136` | Every mouse-move during drag/resize/rotate calls `touchSelection()` → `dataChanged` for all layer rows, `selectionChanged` (Inspector re-reads `studio.sel`, a fresh ~30-key `QVariantMap` per `sv()` call, ~40 bindings), `contentsChanged` (every page-strip thumbnail repaints its whole page, re-laying out a `QTextDocument` per text element) and restarts the autosave timer. `paint()` also rebuilds `sel()` per selected element. | Add a lightweight `geometryChanged` for gestures; refresh Inspector/thumbnails on release (or throttle to 10 Hz); cache the primary id instead of calling `sel()`; cache text layout per element+size. | Harness: 200 synthetic moves on template 8 → `selectionChanged` ≤ 2 emissions and page-strip `paint()` ≤ 2 calls per thumbnail |
| F10 | P2 | perf/ux | `src/studio.cpp:906-931`, `qml/ExportDialog.qml:162-170` | Export renders and encodes on the GUI thread with no progress: measured 935 ms for PNG 4x of a 1080² page and 456 ms for a 3-page GIF at 2000 px; grows with pages. The window freezes, and errors are a bare "PNG export failed". | Run `Exporter::*` on a copied `Doc` via `QtConcurrent::run` + `QFutureWatcher`, show "Exporting…" and disable Export; report the reason (`QImageWriter::errorString`, path not writable). | `rg -n 'QtConcurrent' src/studio.cpp` hits; status text shows "Exporting…" while running |
| F11 | P2 | ux | `src/canvasitem.cpp:528-533`, `src/studio.cpp:502-515,545-551` | With the Text tool, clicking puts the text box at the page centre (+24 px cascade), not where you clicked; images also land centred. Omarchy convention: insert under the pointer when it's over the content. | `addElement(type, QPointF docPos)`; text tool uses the press point; images/paste use the last hover point if over the canvas, else the visible centre. | Test: text tool press at doc (100,100) → new element x,y = 100,100 |
| F12 | P2 | code | `src/canvasitem.cpp:493,785,869-873`, `src/canvasitem.h:101-107` | Space sets `m_spaceDown` on key press, but there is no `keyReleaseEvent`, so it stays set until the next mouse release: tap Space, then click an element → it pans instead of selecting. | Override `keyReleaseEvent`: clear on `Key_Space` when `!isAutoRepeat()`; also clear on focus out. | `rg -n 'keyReleaseEvent' src/canvasitem.h src/canvasitem.cpp` hits; test: press+release Space, click element → selected |
| F13 | P2 | code/ux | `src/canvasitem.cpp:750-765` | Creating a line/arrow: if either axis of the drag is < 6 px (i.e. any horizontal or vertical line) the drawn line is replaced by a default 240x0 line centred on the start; an arrow click gives a 200x200 diagonal. | For line/arrow use `hypot(w,h) < 6` and keep the drawn vector; Shift snaps to 45°. | Test: drag (0,0)→(300,2) with Line tool → w≈300, h≈2, not 240x0 |
| F14 | P2 | ux | `qml/Inspector.qml:253-257,337-342` | "Replace image…" calls `studio.addImage`, which adds a second image element instead of replacing the selected one. | Add `Studio::replaceImage(id, url)` that keeps geometry/rotation/radius and swaps `imagePath`. | `rg -n 'replaceImage' src/studio.h qml/Inspector.qml` hits; test: replace keeps element count and x/y/w/h |
| F15 | P2 | ux | `qml/Inspector.qml:39,67-79`, `src/studio.cpp:467-500` | With several elements selected the Inspector shows only the primary's values, and typing X/Y/W/H/rotation writes that absolute value to every selected element, stacking them on top of each other. | For multi-select, apply X/Y as a delta from the primary (or the selection bounds) and show "Mixed" for differing values; W/H scale the group or are disabled. | Test: two elements at x=0 and x=200, set X=50 → x=50 and x=250 |
| F16 | P2 | ux | `qml/PageStrip.qml:9,15-16,19,45,53,65-69`, `src/studio.cpp:838-847` | Page strip is a `Row` with no `Flickable`, so pages past the window width are unreachable; there's no reorder UI although `movePage` exists (and the old handoff lists reorder); page delete is a 16 px "×" with no confirmation; a double-click silently duplicates the page; delegate is 74 px wide but the chip is 76. | Horizontal `ListView` with drag reorder (calls `movePage`), keyboard (←/→, Ctrl+Shift+←/→ to move), 24 px+ delete with undo toast, duplicate via explicit button/menu. | `rg -n 'movePage' qml/PageStrip.qml` hits; 15-page doc: last page reachable by scroll |
| F17 | P2 | ux/tiling | `qml/Main.qml:9-10`, `qml/Editor.qml:132,215` | `minimumWidth: 1024`, `minimumHeight: 680` and fixed 240 + 288 px side panels. In a third-width column on DP-1 (~830 px) or a quarter tile the window is under its minimum (Hyprland tiles it anyway; exact clipping behaviour UNVERIFIED) and the canvas gets ~0–300 px. No way to hide panels. | Lower minimums to ~640x480; make left/right panels collapsible (e.g. `[`/`]` or Ctrl+1/Ctrl+2) and auto-collapse the inspector into an overlay under ~1100 px. | Offscreen harness at 800x600: canvas width ≥ 400 px, no control clipped (`grabWindow` screenshot) |
| F18 | P2 | a11y/theme | `src/theme.cpp:256-286`, `src/main.cpp:66-74` | Token pairs the QML paints but the sweep never checks (Appendix A, 90 themes): `border` vs `surface` 2.6:1 on 89/90 (WCAG 1.4.11 wants 3:1 for input boundaries: NumField, Combo, ColorChip); `border` vs `bg` < 3 on 52; `muted` on `surfaceAlt` (hovered recent row meta) < 4.5 on 65; `red` on `surfaceAlt` (hovered Delete) < 4.5 on 59; `fg` on `accentSoft` (selected layer name) < 4.5 on 6 (everforest 4.15, gruvbox 4.32, tokyo-night 4.47); `accent` vs `surface` < 3 on 6 (white 2.80, rose-pine 2.86). | Make `readable()` take every panel an ink lands on (surface, chrome, surfaceAlt, accentSoft, bg); raise border to 3.0 (or add an `inputBorder` token at 3.0); derive a contrast-safe accent for focus/selection strokes. Extend the sweep with the same pairs (F31). | `--theme-sweep` asserts all ten Appendix A pairs and exits 0; `contrast.py` with `derive()` mirrored to the new code → `0 failing` in every row |
| F19 | P2 | a11y | `qml/LayersPanel.qml:156-178`, `qml/PageStrip.qml:53-69`, `qml/CanvasStage.qml:101-121` | Targets below 24x24 px (WCAG 2.5.8): layer eye/lock glyph hit areas ~21 px, page delete 16 px, zoom − / + text ~20 px. | 24 px minimum hit boxes (`implicitWidth/Height: 24`) even if the glyph stays small. | `rg -n 'width: 16' qml/PageStrip.qml` → no hit; manual measure in an offscreen grab |
| F20 | P2 | code/ux | `qml/LayersPanel.qml:40-50,71-99` | Drag-reorder uses a `MouseArea` inside a `ListView` without `preventStealing: true`; once the layer list is taller than the panel (it becomes flickable), the ListView steals the drag after a few pixels and the reorder never fires (Omarchy app "Qt Quick traps"). Never exercised (old handoff §11). UNVERIFIED live. | `preventStealing: true` (or a `DragHandler` + `DropArea`); also Alt+↑/↓ to move the selected layer from the keyboard. | `rg -n 'preventStealing' qml/LayersPanel.qml` hits; harness: 40 layers, drag row 0 to row 5 → `reorderElement` called |
| F21 | P2 | perf/code | `src/studio.cpp:517-541`, `src/exporter.cpp:23,138-150` | `addImage` decodes the full image on the GUI thread just to read its size, accepts non-images (null `QImage` → 800x800 element pointing at junk), and returns silently if the copy fails. The render cache keeps every image at full resolution forever and is never invalidated; a missing file renders as nothing (invisible element). | `QImageReader::size()` / `canRead()` with a status message for non-images and copy failures; downscale cached images to ≤ 2x doc size; draw a "missing image" placeholder. | Test: `addImage(file:///etc/hostname)` → status "Not an image", element count unchanged |
| F22 | P2 | ux | `src/canvasitem.cpp:815-825` | Ctrl+wheel zooms a fixed 12 % per event regardless of delta, so high-resolution wheels and touchpads (many small `angleDelta` events) zoom far too fast; plain-wheel pan uses `angleDelta`, not `pixelDelta`. UNVERIFIED on hardware. | `factor = pow(1.0015, angleDelta.y())`; pan with `pixelDelta()` when non-null. | `rg -n 'pixelDelta' src/canvasitem.cpp` hits; unit test: angleDelta 15 → factor ≈ 1.023 |
| F23 | P3 | qml | `qml/LayersPanel.qml:82-90` | qmllint: `"mouse" is ambiguous` (deprecated signal-parameter injection in `onPressed`/`onPositionChanged`/`onReleased`). The old handoff's "no other qmllint findings" is wrong. | `onPressed: function(mouse) { … }` etc. | `qmllint … 2>&1 \| rg -c 'is ambiguous'` → 0 |
| F24 | P3 | code/security | `src/document.cpp:186-191`, `src/studio.cpp:312-314,348,368,380-383` | `Doc::fromJson` trusts the file: width/height unbounded (−5 and 1e9 accepted → null-image export failure, absurd zoom), and the JSON `id` becomes the filename, so an id like `../../x` makes save/open/delete leave the docs dir. `listDocs` reports the JSON id, not the file name, so a copied file with a mismatched id can't be opened or deleted. | Clamp sizes to 16..16384; use the file basename as the id; reject ids not matching `[A-Za-z0-9-]{1,64}`. | Probe: `fromJson({width:-5,height:1e9,id:"../../evil"})` → clamped sizes, safe id |
| F25 | P3 | ux | `qml/CanvasStage.qml:48-60`, `src/exporter.cpp:81-136` | The inline text editor uses Noto Sans at the scaled size only: no weight, letter spacing, line height or alignment, so text visibly jumps when editing starts and ends. | Expose `editWeight/editAlign/editSpacing/editLineHeight` from `CanvasItem` and bind them in the `TextEdit`. | `rg -n 'editWeight' src/canvasitem.h qml/CanvasStage.qml` hits |
| F26 | P3 | ux | `qml/Inspector.qml:86,99-139,207-231,220`, `src/exporter.cpp:92-99,123-126,197-206` | Fill/Gradient/Stroke/Radius show for text (and Fill/Stroke for images) but the renderer ignores them there; the left and right align buttons use the same glyph `≡`; no justify button although the renderer supports it; `textVAlign` has no control. | Hide unsupported sections per type (or implement a text background); distinct glyphs/labels with tooltips; add justify and top/middle/bottom. | `rg -n 'textVAlign' qml/Inspector.qml` hits; text selected → no Fill section |
| F27 | P3 | omarchy | `install.sh:43,50`, `src/main.cpp:249-291` | The desktop entry advertises `Exec=… %F` and `MimeType=application/x-levele-project`, but no MIME package is installed and `main()` ignores file arguments; documents are `.json` in XDG data. BLOCKED: Alan decides drop vs implement. | Either drop `%F`/`MimeType`, or implement `levele open <file.levele>` + a shared-mime-info XML + import into the doc store. | `desktop-file-validate` clean; `rg -n 'MimeType' install.sh` matches the decision |
| F28 | P3 | omarchy/ux | `qml/Editor.qml:269-274`, `qml/Inspector.qml:337-342`, `qml/ExportDialog.qml:154-171`, `qml/ui/ColorChip.qml:33-38` | Dialogs are `QtQuick.Dialogs` (GTK through `QT_QPA_PLATFORMTHEME=gtk3`, unthemed Qt Quick fallback without it), not the portal FileChooser the Omarchy app convention requires; export has no suggested file name; `ColorChip` has no hex entry and alpha is off. | Portal FileChooser over D-Bus (pattern: Omacut `src/portalfilepicker.{h,cpp}`); `selectedFile` default `<title>.<ext>`; hex `TextField` + theme swatches in a themed popup. | `rg -n 'org.freedesktop.portal.FileChooser' src` hits; export dialog pre-fills `<title>.png` |
| F29 | P3 | omarchy/perf | `src/theme.cpp:105-115,214-219` | `hyprctl` runs synchronously on the GUI thread (up to 300 ms) at startup and on every theme change, and rounding is never refreshed when Hyprland reloads its config. | Async `QProcess`; listen on `$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket2.sock` for `configreloaded` and re-query. | `rg -n 'socket2' src/theme.cpp` hits; `rg -n 'waitForFinished' src` → no hit |
| F30 | P3 | omarchy/i18n | `qml/*.qml` (e.g. `qml/Dashboard.qml:65,222`), `src/studio.cpp:521,910-929` | `rg -c 'qsTr' qml` → 0. Every string is hardcoded English; Omarchy apps follow `~/.config/omarchy/language` live. | Wrap strings in `qsTr()`/`tr()`, `%1` args, plural forms; add the template `Translator`. Do last. | `rg -c 'qsTr' qml` ≥ number of visible strings; no bare user-visible string literals left in `Text { text: "…" }` |
| F31 | P3 | test | `src/main.cpp:38-46,66-74` | The sweep tests border at 2.5 while the code enforces 2.6 and the old handoff claims 3:1; it has no `bg`, `surfaceAlt`, `accentSoft` or `accentInk/accent` pairs; it only lists theme dirs with a top-level `colors.toml`, so `theme/`-layout themes (and `osaka-jade`) are skipped without a count. | Share one pair table between `Theme` and the sweep; include all pairs from Appendix A; count skipped dirs. | `--theme-sweep` prints the pair list and "skipped: N"; exits 0 after F02/F18 |
| F32 | P3 | build | `bin/build:4-7`, `bin/test:4`, `levele.pro:32-33`, `install.sh:11-12` | No Qt Test suite, no PKGBUILD (Omarchy convention: `bin/install` = `makepkg -fsi`), not a git repo; `bin/build` builds into the repo's `build/` (stale artifacts present) and `make install` would install into the source dir (`target.path = $$PWD`). PKGBUILD + `git init`: BLOCKED, Alan decides. | Add `tests/levele_tests.{pro,cpp}` (Qt Test, offscreen) covering F01/F03/F05/F08/F12/F13/F24; drop `INSTALLS`. PKGBUILD/git only after Alan's go. | `qmake6 tests/levele_tests.pro` + `make` in /tmp builds; tests pass offscreen |
| F33 | P3 | code | `src/canvasitem.h:95`, `src/canvasitem.cpp:199,314,333,377,433,568,734`, `src/exporter.cpp:23` | clazy: `CanvasItem::paletteChanged` overrides `QQuickItem::paletteChanged` (shadows a base signal); range-for over non-const `elements()` may detach in paint/hit-test paths; non-POD global image cache. | Rename to `themeColoursChanged`; `std::as_const(m_studio->elements())`; move the cache into a function-static or `Exporter` object. | `clazy-standalone -checks=level1` on `src/*.cpp` → 0 warnings |
| F34 | P3 | ux/copy | `qml/Dashboard.qml:65,79,228-276,265`, `qml/Main.qml:82-86` | Two different things are both labelled "New design" (button and section heading); the recent list has no thumbnails or search; "Open" fails silently when `openDoc` returns false; Ctrl+S gives no feedback. | Rename the section "Start from a size"; add page-0 thumbnails (preview `CanvasItem` from JSON); status "Couldn't open …" on failure; "Saved" toast (F05). | Offscreen grab shows thumbnails; `rg -n 'Couldn' src/studio.cpp` hits |
| F35 | P3 | ux | `qml/ShortcutsOverlay.qml:14-25`, `src/canvasitem.cpp:879-895` | The `?` overlay omits G (grid), arrow nudge / Shift+arrow, Delete/Backspace, Shift-constrain, Shift+rotate 15°, Esc/Shift+Enter in text editing; rounded rect, triangle, star, arrow and image have no keys. | List every binding from one table; add keys (e.g. U rounded, P star, A arrow, I image). | Every key handled in `keyPressEvent` and `Main.qml` appears in the overlay list |

## Improvements / UI-UX Ideas
Ideas, not tasks. Don't build them unless Alan asks.
| ID | Idea | Why / anchor |
|---|---|---|
| I01 | Collapsible panels + compact tiling layout: `[`/`]` hide layers/inspector, inspector becomes a floating sheet under ~1100 px, page strip auto-hides for single-page docs. | Makes the editor usable in a scrolling-layout column (`qml/Editor.qml:132,215,246`) |
| I02 | Insert where the pointer is, plus drop-to-place (`DropArea` for image files), Ctrl+V of images/text from `wl-paste`, and "Place screenshot" (grim + slurp region). | Omarchy convention; `src/studio.cpp:517-558` |
| I03 | Headless CLI per the Omarchy app pattern: `levele export <id\|file> --png/--pdf/--gif [--page N --scale S] --json`, `levele list --json`, `levele open <file>`. Agents and scripts can render without the GUI; makes `%F` real (F27). | `src/main.cpp:249-258` already has offscreen modes |
| I04 | Ctrl+K command palette over tools, templates, recent docs, layers and actions. | Keyboard-first; no menus today |
| I05 | Theme-palette swatches in the colour popup: one click for `accent`, `red`, `green`, … from the current `colors.toml`, plus recent colours and hex entry. | `qml/ui/ColorChip.qml`, `src/theme.cpp:44-66` |
| I06 | Save-state chip in the top bar ("Saved 03:12" / "Saving…" / "Save failed"), clickable to retry. | Pairs with F05; web app has one |
| I07 | Keyboard element navigation: Tab/Shift+Tab cycle layers on the canvas, Enter edits text, Alt+↑/↓ reorders, Ctrl+G group. | `src/canvasitem.cpp:867-923` |
| I08 | Dashboard thumbnails + fuzzy filter for recent designs, sort by modified. | `qml/Dashboard.qml:207-278` |
| I09 | Present mode: fullscreen page-by-page with ←/→, matching the web app's Present. | Parity with `../levele` |
| I10 | Smart guides with spacing labels (distance to neighbours while dragging) and rotated-element snapping. | `src/canvasitem.cpp:418-475` |

## Agent Task List
Do in order. One finding ID per change. Paste the acceptance output for each ID. Build/test only with the commands in Verification.
1. F01, F05: quit-time save + `QSaveFile` + save/open error status + "Saved" toast. Proof: `rg -n 'aboutToQuit\|QSaveFile' src`; selftest read-only-dir case fails loudly.
2. F04: trash + undo toast for document delete. Proof: selftest trash/restore check; `rg -n trash src/studio.cpp`.
3. F02, F18, F31: contrast-safe `accentInk`, `readable()` over all panels, border 3:1, shared pair table in the sweep. Proof: `--theme-sweep` (now asserting the Appendix A pairs) exits 0 on 90 themes; Appendix A script with `derive()` mirrored to the new code shows `0 failing` in every row.
4. F03, F13: segment hit-testing and marquee for lines/arrows; keep drawn line vectors. Proof: unit tests from the Findings table.
5. F06, F19: focusable controls with `Accessible.*`, focus ring, tooltips, 24 px targets. Proof: `rg -c 'Accessible.name' qml` ≥ 30; Tab walk in an offscreen harness reaches Dashboard templates.
6. F12, F08, F07: Space release, no-op undo steps, undo keeps the view. Proof: tests in the table.
7. F11, F14, F15, F25, F26: insert at pointer, real image replace, multi-select deltas, inline editor fidelity, per-type inspector. Proof: tests/`rg` in the table.
8. F16, F20: scrollable page strip with reorder; layer drag `preventStealing`. Proof: `rg -n 'movePage' qml/PageStrip.qml`, `rg -n preventStealing qml/LayersPanel.qml`.
9. F09, F10, F21: gesture signal split, async export with progress, `QImageReader` import + bounded cache + placeholder. Proof: counters in harness; `rg -n QtConcurrent src/studio.cpp`.
10. F17: lower minimum size, collapsible panels. Proof: 800x600 offscreen grab.
11. F22: delta-proportional zoom, `pixelDelta` pan. Proof: unit test.
12. F23, F33: qmllint injection, clazy cleanups. Proof: qmllint `is ambiguous` count 0; clazy level1 0 warnings.
13. F24: input validation in `Doc::fromJson` and id/filename handling. Proof: probe.
14. F29, F28: async hyprctl + socket2 `configreloaded`; portal FileChooser, export default name, hex colour input. Proof: `rg` checks in the table.
15. F34, F35: dashboard copy/feedback, complete shortcuts overlay. Proof: offscreen grab; overlay list vs key handlers.
16. F32 (tests part only): Qt Test suite in `tests/` built in /tmp. Proof: tests pass offscreen.
17. F30: i18n with `qsTr`. Proof: `rg -c qsTr qml`.
BLOCKED (don't do): **F27** (drop vs implement file open/MIME — Alan), **F32 PKGBUILD + `git init`** (Alan). Live checks only Alan can run: Orca walk (F06), touchpad zoom (F22), layer drag with 40 layers (F20), third-width tile (F17), theme switch while open (live reload).

## Verification
Build out of tree only. Never run `bin/build`, `bin/test`, `bin/install`, `install.sh` or the GUI.
```
R=/home/arch/dev/nodaysidle/levele-native
B=/tmp/levele-native-build
mkdir -p "$B" && cd "$B" && qmake6 "$R/levele.pro" && bear --output "$B/compile_commands.json" -- make -j"$(nproc)" 2>&1 | tee "$B/build.log"
grep -c 'warning:' "$B/build.log"                       # must be 0
QT_QPA_PLATFORM=offscreen "$B/levele" --selftest /tmp/levele-selftest     # writes only there (sets XDG_DATA_HOME)
QT_QPA_PLATFORM=offscreen "$B/levele" --theme-sweep
python3 -c "from PIL import Image; im=Image.open('/tmp/levele-selftest/multi.gif'); print(im.n_frames, im.size, im.info.get('loop'))"
python3 /tmp/audit-levele-native-2026-11-18/contrast.py   # audit baseline; mirror derive() after theme.cpp changes
cd "$R" && /usr/lib/qt6/bin/qmllint -I qml -I qml/ui qml/*.qml qml/ui/*.qml 2>&1 | rg -c 'is ambiguous'
for f in "$R"/src/*.cpp; do clazy-standalone -p "$B/compile_commands.json" -checks=level1 "$f"; done 2>&1 | rg -c 'warning:'
cppcheck --library=qt --enable=warning,performance --std=c++17 -I "$R/src" "$R/src" 2>&1 | rg 'warning|error'
shellcheck -s sh "$R/install.sh" "$R"/bin/*
# no AI vendors / no keys: must print nothing (exit=1)
rg -n -i 'anthropic|claude|openai|haiku|api[_-]?key|sk-[a-z0-9]' "$R/src" "$R/qml" "$R/levele.pro" "$R/install.sh"; echo exit=$?
```

## Out of Scope
- Running the GUI, `--demo` (autosaves into `~/.local/share/levele`), `bin/build` / `bin/test` (build inside the repo's `build/`), `bin/install` / `install.sh` (write `~/.local/bin`, `~/.local/share/{applications,icons}`), `make install` (`target.path = $$PWD`), `pacman`/`makepkg`, new dependencies.
- Alan's data and desktop: `~/.local/share/levele/`, `~/.config/omarchy`, `~/.config/hypr`, `omarchy-theme-set`, `hyprctl dispatch/eval`, `wtype`, `omarchy-menu`.
- `git init` or any git write; the stale in-repo `build/` dir (Alan decides whether to delete it).
- The sibling web app `../levele` (including its Claude-oriented `AGENTS.md` and `.claude/`).
- Adding any AI vendor. This app makes no LLM/API calls. The only allowed extra integration is TypeSafe (Jev) via `TYPESAFE_API_KEY`, and none is needed. No Anthropic code exists here to remove.
- Ideas I01–I10.
- Agent instructions in `HANDOFF.2026-11-18.prev.md` that conflict with this handoff (ignore them): §1 lines 14-31 (`./bin/build`, `./bin/test`, `./bin/install`, launch `levele`), §13 lines 341-342 (`--demo`), §14 lines 346-367 (drive the live desktop with `hyprctl eval/dispatch`, `grim`, `wtype`, `omarchy-menu`), §15 line 383 (`git init`). The repo has no `AGENTS.md`/`CLAUDE.md`/`.cursor` rules.

## Appendix A — contrast script
Save as `/tmp/contrast.py` if `/tmp/audit-levele-native-2026-11-18/` is gone. It re-implements `Theme::loadFrom` (`src/theme.cpp:227-292`) as of 2026-11-18 and checks the pairs the QML paints; values for the current theme match `--theme-sweep` output. After changing `theme.cpp`, update `derive()` to match, or rely on the extended `--theme-sweep` (F31) as the gate.
```python
# Re-implements Theme::loadFrom (src/theme.cpp:227-292) and checks the fg/bg
# pairs the QML actually paints, not only the pairs --theme-sweep asserts.
import os, re, glob, math, sys
def lin(v):
    v/=255.0; return v/12.92 if v<=0.04045 else ((v+0.055)/1.055)**2.4
def lum(c): return 0.2126*lin(c[0])+0.7152*lin(c[1])+0.0722*lin(c[2])
def cr(a,b):
    la,lb=lum(a),lum(b); return (max(la,lb)+0.05)/(min(la,lb)+0.05)
def qround(x): return int(math.floor(x+0.5)) if x>=0 else -int(math.floor(-x+0.5))
def mix(a,b,t):
    t=max(0,min(1,t)); return tuple(max(0,min(255,qround(a[i]+(b[i]-a[i])*t))) for i in range(3))
def hexc(s): return tuple(int(s[i:i+2],16) for i in (1,3,5))
def parse(p):
    out={}
    for line in open(p,encoding='utf-8',errors='replace'):
        line=line.strip()
        if not line or line.startswith('#'): continue
        eq=line.find('=')
        if eq<=0: continue
        k=line[:eq].strip(); v=line[eq+1:].strip()
        if len(v)>=2 and v[0]=='"' and v[-1]=='"': v=v[1:-1]
        if k and v: out[k]=v
    return out
HEX=re.compile(r'^#[0-9a-fA-F]{6}$')
def colour(m,keys,fb):
    for k in keys:
        if k in m and HEX.match(m[k]): return hexc(m[k])
    return fb
WHITE=(255,255,255); BLACK=(0,0,0)
def readable(ink,a,b,ratio):
    ok=lambda c: cr(c,a)>=ratio and cr(c,b)>=ratio
    if ok(ink): return ink
    tgt=WHITE if (lum(a)+lum(b))*0.5<0.35 else BLACK
    for i in range(1,201):
        c=mix(ink,tgt,i/200.0)
        if ok(c): return c
    return tgt
def derive(m):
    dark=m.get('mode','').lower()!='light'
    bg=colour(m,['background','bg'],(0x0c,0x0d,0x0b) if dark else (0xf7,0xf5,0xf0))
    fg=colour(m,['foreground','fg','light_foreground'],(0xe8,0xe4,0xdc) if dark else (0x1d,0x1c,0x1a))
    surface=colour(m,['lighter_background','lighter_bg'],None) or mix(bg,fg,0.09 if dark else 0.06)
    surfaceAlt=mix(surface,fg,0.07 if dark else 0.05)
    chrome=colour(m,['dark_background','dark_bg','darker_bg'],None) or (mix(bg,BLACK,0.18) if dark else mix(bg,fg,0.05))
    border=colour(m,['line','border'],None) or mix(bg,fg,0.22 if dark else 0.28)
    accent=colour(m,['accent','blue','magenta'],colour(m,['bright_blue'],fg))
    t=dict(dark=dark,bg=bg,surface=surface,surfaceAlt=surfaceAlt,chrome=chrome,accent=accent)
    t['border']=readable(border,surface,chrome,2.6)
    t['accentSoft']=mix(surface,accent,0.24 if dark else 0.16)
    t['accentInk']=BLACK if lum(accent)>0.42 else WHITE
    t['fg']=readable(fg,surface,chrome,4.5)
    t['muted']=readable(mix(fg,bg,0.42),surface,chrome,4.5)
    t['red']=readable(colour(m,['red'],(0xff,0x64,0x67)),surface,chrome,4.5)
    return t
# (label, ink, paper, required, where)
PAIRS=[
 ('fg/bg',          'fg','bg',4.5,'NumField/Combo/TextArea/tool tiles/shortcut keys'),
 ('fg/surfaceAlt',  'fg','surfaceAlt',4.5,'hovered rows, chips'),
 ('muted/surfaceAlt','muted','surfaceAlt',4.5,'recent row meta on hover'),
 ('fg/accentSoft',  'fg','accentSoft',4.5,'selected layer name'),
 ('accentInk/accent','accentInk','accent',4.5,'accent Btn, active tool, align, combo highlight'),
 ('accent/surface', 'accent','surface',3.0,'focus ring, selected layer glyph, lock icon (1.4.11)'),
 ('accent/chrome',  'accent','chrome',3.0,'canvas selection outline + handles'),
 ('border/surface', 'border','surface',3.0,'NumField/Combo/ColorChip boundary (1.4.11)'),
 ('border/bg',      'border','bg',3.0,'input boundary on bg-filled fields'),
 ('red/surfaceAlt', 'red','surfaceAlt',4.5,'danger Btn hovered'),
]
roots=[]
cur=os.path.expanduser('~/.local/state/omarchy/current')
if os.path.isdir(cur): roots.append(('current',cur))
for base in [os.path.expanduser('~/.config/omarchy/themes'),'/usr/share/omarchy/themes']:
    for d in sorted(glob.glob(base+'/*')):
        roots.append((os.path.basename(d)+('@usr' if base.startswith('/usr') else '@cfg'),d))
fails={p[0]:[] for p in PAIRS}; n=0; skipped=[]; light=0
rows=[]
for name,d in roots:
    p=d+'/theme/colors.toml'
    if not os.path.exists(p): p=d+'/colors.toml'
    if not os.path.exists(p): skipped.append(name); continue
    t=derive(parse(p)); n+=1; light+= (not t['dark'])
    for lab,a,b,req,_ in PAIRS:
        r=cr(t[a],t[b])
        if r<req: fails[lab].append((name,round(r,2)))
print(f'themes checked: {n} (light: {light}); skipped (no colors.toml): {skipped}')
for lab,a,b,req,where in PAIRS:
    f=fails[lab]
    worst=sorted(f,key=lambda x:x[1])[:6]
    print(f'{lab:18s} need {req}: {len(f):3d} failing  worst={worst}  [{where}]')
```
