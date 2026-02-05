#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/dist/desktop/macos"
APP_NAME="Konflikt"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
DMG_NAME="Konflikt-Installer"
DMG_PATH="$BUILD_DIR/$DMG_NAME.dmg"
VOLUME_NAME="Konflikt Installer"

# Check if app exists
if [ ! -d "$APP_BUNDLE" ]; then
    echo "Error: $APP_BUNDLE not found. Run build.sh first."
    exit 1
fi

echo "Creating DMG installer..."

# Remove old DMG if exists
rm -f "$DMG_PATH"

# Create temporary directory for DMG contents
DMG_TEMP="$BUILD_DIR/dmg-temp"
rm -rf "$DMG_TEMP"
mkdir -p "$DMG_TEMP"

# Copy app to temp directory
cp -R "$APP_BUNDLE" "$DMG_TEMP/"

# Create symbolic link to Applications folder
ln -s /Applications "$DMG_TEMP/Applications"

# Create a README file
cat > "$DMG_TEMP/README.txt" << 'EOF'
Konflikt - Cross-Platform KVM Switch

Installation:
1. Drag Konflikt.app to the Applications folder
2. Open Konflikt from Applications
3. Grant Accessibility permissions when prompted
4. Configure your server address in Preferences

For more information, visit:
https://github.com/Andersbakken/konflikt
EOF

# Create DMG
echo "Building DMG..."
hdiutil create -volname "$VOLUME_NAME" \
    -srcfolder "$DMG_TEMP" \
    -ov -format UDZO \
    "$DMG_PATH"

# Clean up
rm -rf "$DMG_TEMP"

echo ""
echo "DMG created: $DMG_PATH"
echo ""
echo "To install:"
echo "  1. Open the DMG"
echo "  2. Drag Konflikt to Applications"
echo "  3. Eject the DMG"
