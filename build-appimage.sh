#!/usr/bin/env bash
set -Eeuo pipefail

APP_NAME="XdrTablet"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build-appimage"
DIST_DIR="$ROOT_DIR/dist"
APPDIR="$DIST_DIR/${APP_NAME}.AppDir"
TOOLS_DIR="$ROOT_DIR/.appimage-tools"
PACKAGING_DIR="$ROOT_DIR/packaging"

fail() {
    echo "FEHLER: $*" >&2
    exit 1
}

for cmd in cmake ninja wget file; do
    command -v "$cmd" >/dev/null 2>&1 || fail "'$cmd' fehlt."
done

ARCH="$(uname -m)"
case "$ARCH" in
    x86_64|amd64)
        TOOL_ARCH="x86_64"
        ;;
    *)
        fail "Dieses Skript ist derzeit für x86_64 erstellt; erkannt wurde: $ARCH"
        ;;
esac

mkdir -p "$DIST_DIR" "$TOOLS_DIR" "$PACKAGING_DIR"
rm -rf "$APPDIR"

echo "==> Release-Version bauen"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

BINARY="$BUILD_DIR/bin/$APP_NAME"
if [[ ! -x "$BINARY" ]]; then
    BINARY="$(find "$BUILD_DIR" -type f -name "$APP_NAME" -perm -u+x | head -n 1 || true)"
fi
[[ -n "${BINARY:-}" && -x "$BINARY" ]] || fail "Ausführbare Datei '$APP_NAME' wurde nicht gefunden."

DESKTOP_FILE="$PACKAGING_DIR/${APP_NAME}.desktop"
ICON_FILE="$PACKAGING_DIR/${APP_NAME}.svg"

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=XdrTablet
Comment=TCP-Client für einen XDR-kompatiblen FM-Tuner
Exec=XdrTablet
Icon=XdrTablet
Terminal=false
Categories=AudioVideo;Audio;Network;
StartupNotify=true
EOF

cat > "$ICON_FILE" <<'EOF'
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <defs>
    <linearGradient id="metal" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#eeeeea"/>
      <stop offset="0.55" stop-color="#c8c8c2"/>
      <stop offset="1" stop-color="#9f9f99"/>
    </linearGradient>
  </defs>
  <rect x="8" y="8" width="240" height="240" rx="30" fill="url(#metal)" stroke="#252525" stroke-width="8"/>
  <rect x="30" y="48" width="196" height="76" rx="8" fill="#25282a"/>
  <path d="M43 96h170" stroke="#e8d7a2" stroke-width="4"/>
  <path d="M62 78v36M96 78v36M130 78v36M164 78v36M198 78v36" stroke="#e8d7a2" stroke-width="3"/>
  <circle cx="178" cy="181" r="39" fill="#bababa" stroke="#303030" stroke-width="6"/>
  <circle cx="178" cy="181" r="18" fill="#8d8d8d"/>
  <text x="40" y="190" font-family="sans-serif" font-size="32" font-weight="bold" fill="#202020">XDR</text>
</svg>
EOF

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-${TOOL_ARCH}.AppImage"
QT_PLUGIN="$TOOLS_DIR/linuxdeploy-plugin-qt-${TOOL_ARCH}.AppImage"

if [[ ! -f "$LINUXDEPLOY" ]]; then
    echo "==> linuxdeploy herunterladen"
    wget -O "$LINUXDEPLOY" \
      "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${TOOL_ARCH}.AppImage"
fi

if [[ ! -f "$QT_PLUGIN" ]]; then
    echo "==> Qt-Plugin herunterladen"
    wget -O "$QT_PLUGIN" \
      "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${TOOL_ARCH}.AppImage"
fi

chmod +x "$LINUXDEPLOY" "$QT_PLUGIN"
export PATH="$TOOLS_DIR:$PATH"

# Das Projekt bettet QML über qt_add_qml_module ein. Der Qt-Plugin-Scanner
# braucht trotzdem den Quellordner, um alle importierten QML-Module zu finden.
export QML_SOURCES_PATHS="$ROOT_DIR/qml"

if command -v qmake6 >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake6)"
fi

echo "==> AppDir und AppImage erzeugen"
cd "$DIST_DIR"
rm -f ./*.AppImage

# APPIMAGE_EXTRACT_AND_RUN vermeidet Probleme, wenn FUSE für die
# Verpackungswerkzeuge auf dem Build-Rechner nicht verfügbar ist.
APPIMAGE_EXTRACT_AND_RUN=1 "$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$BINARY" \
    --desktop-file "$DESKTOP_FILE" \
    --icon-file "$ICON_FILE" \
    --plugin qt \
    --output appimage

OUTPUT="$(find "$DIST_DIR" -maxdepth 1 -type f -name '*.AppImage' | head -n 1 || true)"
[[ -n "$OUTPUT" ]] || fail "linuxdeploy hat keine AppImage-Datei erzeugt."

chmod +x "$OUTPUT"

echo
echo "Fertig:"
echo "$OUTPUT"
echo
echo "Test:"
echo "\"$OUTPUT\""
