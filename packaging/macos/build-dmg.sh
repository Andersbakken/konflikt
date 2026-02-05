#!/bin/bash
# Build Konflikt macOS DMG
# Creates a drag-and-drop installer disk image

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DMG_DIR="$PROJECT_ROOT/build-dmg"
APP_NAME="Konflikt"
DMG_NAME="Konflikt"
VERSION=$(git -C "$PROJECT_ROOT" describe --tags --always 2>/dev/null || echo "1.0")

echo "Building Konflikt macOS DMG..."
echo "Version: $VERSION"

# Build the app first if needed
if [ ! -d "$BUILD_DIR/dist/$APP_NAME.app" ]; then
    echo "Building application..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake "$PROJECT_ROOT" -G Ninja -DBUILD_UI=OFF
    ninja
fi

# Verify app exists
if [ ! -d "$BUILD_DIR/dist/$APP_NAME.app" ]; then
    echo "Error: $APP_NAME.app not found in $BUILD_DIR/dist/"
    exit 1
fi

# Clean DMG build directory
rm -rf "$DMG_DIR"
mkdir -p "$DMG_DIR"

# Create staging directory for DMG contents
STAGING="$DMG_DIR/staging"
mkdir -p "$STAGING"

# Copy app bundle
echo "Copying application bundle..."
cp -R "$BUILD_DIR/dist/$APP_NAME.app" "$STAGING/"

# Copy UI resources if available
if [ -d "$PROJECT_ROOT/dist/ui" ]; then
    echo "Copying UI resources..."
    mkdir -p "$STAGING/$APP_NAME.app/Contents/Resources/ui"
    cp -R "$PROJECT_ROOT/dist/ui/"* "$STAGING/$APP_NAME.app/Contents/Resources/ui/"
fi

# Create symlink to Applications folder
ln -s /Applications "$STAGING/Applications"

# Create DMG
DMG_PATH="$PROJECT_ROOT/dist/$DMG_NAME-$VERSION.dmg"
TEMP_DMG="$DMG_DIR/temp.dmg"

echo "Creating DMG..."

# Create temporary DMG
hdiutil create -srcfolder "$STAGING" -volname "$APP_NAME" -fs HFS+ \
    -fsargs "-c c=64,a=16,e=16" -format UDRW "$TEMP_DMG"

# Mount it
echo "Configuring DMG appearance..."
MOUNT_DIR=$(hdiutil attach -readwrite -noverify "$TEMP_DMG" | grep -E '^/dev/' | tail -1 | awk '{print $3}')

if [ -n "$MOUNT_DIR" ]; then
    # Set window appearance using AppleScript
    osascript << APPLESCRIPT
tell application "Finder"
    tell disk "$APP_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set bounds of container window to {400, 100, 900, 400}
        set theViewOptions to the icon view options of container window
        set arrangement of theViewOptions to not arranged
        set icon size of theViewOptions to 80
        set position of item "$APP_NAME.app" of container window to {120, 150}
        set position of item "Applications" of container window to {380, 150}
        close
        open
        update without registering applications
        delay 2
        close
    end tell
end tell
APPLESCRIPT

    # Unmount
    sync
    hdiutil detach "$MOUNT_DIR" -quiet
fi

# Convert to compressed DMG
echo "Compressing DMG..."
rm -f "$DMG_PATH"
hdiutil convert "$TEMP_DMG" -format UDZO -imagekey zlib-level=9 -o "$DMG_PATH"

# Clean up
rm -rf "$DMG_DIR"

echo ""
echo "DMG created: $DMG_PATH"
echo ""

# Code signing info
if [ -z "$APPLE_DEVELOPER_ID" ]; then
    echo "Note: DMG is unsigned. To sign for distribution:"
    echo "  codesign --deep --force --sign 'Developer ID Application: Your Name' '$STAGING/$APP_NAME.app'"
    echo "  Then rebuild the DMG."
    echo ""
    echo "For notarization:"
    echo "  xcrun notarytool submit '$DMG_PATH' --apple-id YOUR_APPLE_ID --team-id YOUR_TEAM_ID --password YOUR_APP_PASSWORD"
fi

echo "Done!"
