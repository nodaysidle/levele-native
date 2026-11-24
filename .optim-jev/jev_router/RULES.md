# Codebase context for model routing

Written by `/jev-route init`; edit freely. `## General` facts go with every plan;
a `## <path>` section is sent only when a planned file is under that path.

## General

- build-harness/, build-tests/, and build/ hold generated qmake/moc outputs; never edit them by hand, rebuild via ./bin/build or qmake6.
- graft/ is a local regenerable graph cache (not committed); regenerate with 'graft build' instead of editing its markdown cards.
- C++ builds with -Wall -Wextra under C++17 and Qt Quick 6 Basic style; run ./bin/test after C++ or QML changes.

## levele.pro

- Adding or renaming files in src/ requires updating levele.pro, tests/levele_tests.pro, and tests/qml-harness/ because tests compile app sources directly.

## qml/

- All controls are custom-painted from theme.* tokens with Accessible.* metadata and qsTr() strings; keep 24px minimum tap targets and commit pending edits before closing.

## resources.qrc

- Every new or renamed QML file or asset under qml/ or assets/ must be registered in resources.qrc or Qt will fail to load it at runtime.

## src/

- Canvas (GUI thread) and background exports (QtConcurrent) must keep separate image/QTextDocument caches; no worker-thread QObject may outlive QGuiApplication.
- All document saves and PNG/PDF/GIF exports must commit atomically via QSaveFile and guard against locked layers, hostile JSON, and non-regular image files.

## src/studio.cpp

- Lines and arrows store signed w/h vectors; align/distribute and Inspector property edits must preserve sign and operate via Element::rect().

## src/theme.cpp

- tests/contrast.py re-implements Theme::loadFrom() contrast logic; changes to colour derivation or WCAG contrast pairs must stay in sync with tests/contrast.py.
