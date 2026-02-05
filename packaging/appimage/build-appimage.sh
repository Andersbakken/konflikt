#!/bin/bash
# Build Konflikt AppImage
# Requires: linuxdeploy, linuxdeploy-plugin-appimage

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-appimage"
APPDIR="$BUILD_DIR/AppDir"

echo "Building Konflikt AppImage..."
echo "Project root: $PROJECT_ROOT"

# Check for linuxdeploy
if ! command -v linuxdeploy &> /dev/null; then
    echo "linuxdeploy not found. Downloading..."
    LINUXDEPLOY="$BUILD_DIR/linuxdeploy-x86_64.AppImage"
    if [ ! -f "$LINUXDEPLOY" ]; then
        mkdir -p "$BUILD_DIR"
        wget -O "$LINUXDEPLOY" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
        chmod +x "$LINUXDEPLOY"
    fi
else
    LINUXDEPLOY="linuxdeploy"
fi

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Build the project
echo "Configuring with CMake..."
cmake "$PROJECT_ROOT" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_UI=OFF

echo "Building..."
ninja

# Create AppDir structure
echo "Creating AppDir..."
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "$BUILD_DIR/dist/konflikt" "$APPDIR/usr/bin/"

# Copy desktop file
cp "$SCRIPT_DIR/konflikt.desktop" "$APPDIR/usr/share/applications/"

# Create a simple icon (SVG)
cat > "$APPDIR/usr/share/icons/hicolor/256x256/apps/konflikt.svg" << 'ICONEOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <rect width="256" height="256" rx="32" fill="#2563eb"/>
  <rect x="40" y="60" width="70" height="50" rx="8" fill="white"/>
  <rect x="146" y="60" width="70" height="50" rx="8" fill="white"/>
  <rect x="40" y="146" width="70" height="50" rx="8" fill="white"/>
  <rect x="146" y="146" width="70" height="50" rx="8" fill="white"/>
  <circle cx="128" cy="128" r="20" fill="#1e40af"/>
  <line x1="110" y1="85" x2="128" y2="108" stroke="#1e40af" stroke-width="4"/>
  <line x1="146" y1="85" x2="128" y2="108" stroke="#1e40af" stroke-width="4"/>
  <line x1="110" y1="171" x2="128" y2="148" stroke="#1e40af" stroke-width="4"/>
  <line x1="146" y1="171" x2="128" y2="148" stroke="#1e40af" stroke-width="4"/>
</svg>
ICONEOF

# Also copy as PNG placeholder (linuxdeploy prefers PNG)
cp "$APPDIR/usr/share/icons/hicolor/256x256/apps/konflikt.svg" "$APPDIR/konflikt.svg"

# Create AppRun if linuxdeploy isn't available
if [ ! -x "$LINUXDEPLOY" ] && ! command -v linuxdeploy &> /dev/null; then
    echo "Creating manual AppRun..."
    cat > "$APPDIR/AppRun" << 'APPRUNEOF'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
exec "${HERE}/usr/bin/konflikt" "$@"
APPRUNEOF
    chmod +x "$APPDIR/AppRun"

    # Copy desktop and icon to root
    cp "$APPDIR/usr/share/applications/konflikt.desktop" "$APPDIR/"

    echo "Manual AppDir created at $APPDIR"
    echo "To create AppImage, run: appimagetool $APPDIR"
else
    # Use linuxdeploy to create AppImage
    echo "Creating AppImage with linuxdeploy..."

    export VERSION=$(git -C "$PROJECT_ROOT" describe --tags --always 2>/dev/null || echo "1.0")

    "$LINUXDEPLOY" \
        --appdir "$APPDIR" \
        --desktop-file "$APPDIR/usr/share/applications/konflikt.desktop" \
        --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/konflikt.svg" \
        --output appimage

    # Move AppImage to dist
    mv Konflikt*.AppImage "$PROJECT_ROOT/dist/" 2>/dev/null || true

    echo ""
    echo "AppImage created in $PROJECT_ROOT/dist/"
fi

echo "Done!"
