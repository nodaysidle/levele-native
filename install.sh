#!/bin/sh
# Per-user install for Levele. Idempotent; no sudo, nothing outside $HOME.
set -eu

SRC_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PREFIX="${PREFIX:-$HOME/.local}"
BIN_DIR="$PREFIX/bin"
APP_DIR="$PREFIX/share/applications"
ICON_SCALABLE="$PREFIX/share/icons/hicolor/scalable/apps"

BIN_SRC="$SRC_DIR/build/levele"
[ -x "$BIN_SRC" ] || BIN_SRC="$SRC_DIR/levele"
SVG_SRC="$SRC_DIR/assets/levele.svg"

if [ ! -x "$BIN_SRC" ]; then
    echo "install: $BIN_SRC not found — run 'bin/build' first" >&2
    exit 1
fi

mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_SCALABLE"

install -m 755 "$BIN_SRC" "$BIN_DIR/levele"
install -m 644 "$SVG_SRC" "$ICON_SCALABLE/levele.svg"

# Raster sizes as well as the scalable SVG: menus and docks pick whichever
# matches their render size, and the Omarchy app index prefers SVGs first.
for size in 16 24 32 48 64 128 256 512; do
    dir="$PREFIX/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$dir"
    if command -v magick >/dev/null 2>&1; then
        magick -background none "$SVG_SRC" -resize "${size}x${size}" "$dir/levele.png"
    elif command -v rsvg-convert >/dev/null 2>&1; then
        rsvg-convert -w "$size" -h "$size" "$SVG_SRC" -o "$dir/levele.png"
    fi
done

cat > "$APP_DIR/com.nodaysidle.levele.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Levele
GenericName=Design Studio
Comment=Layered design studio: shapes, text, images, pages, PNG/PDF/GIF export
Exec=$BIN_DIR/levele
Icon=levele
Terminal=false
Categories=Graphics;2DGraphics;VectorGraphics;
StartupWMClass=com.nodaysidle.levele
StartupNotify=true
Keywords=design;canvas;layers;studio;export;template;levele;omarchy;
EOF
chmod 644 "$APP_DIR/com.nodaysidle.levele.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
fi

echo "installed: $BIN_DIR/levele"
echo "menu entry: $APP_DIR/com.nodaysidle.levele.desktop"
