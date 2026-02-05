#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
SOURCE_DIR="$SCRIPT_DIR/Konflikt"
BUILD_DIR="$PROJECT_ROOT/dist/desktop/macos"
APP_NAME="Konflikt"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"

echo "Building Konflikt.app..."
echo "Source: $SOURCE_DIR"
echo "Output: $APP_BUNDLE"

# Create build directory
mkdir -p "$BUILD_DIR"

# Clean previous build
rm -rf "$APP_BUNDLE"

# Create app bundle structure
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

# Copy Info.plist
cp "$SOURCE_DIR/Info.plist" "$APP_BUNDLE/Contents/"

# Compile Swift sources
SWIFT_FILES=(
    "$SOURCE_DIR/main.swift"
    "$SOURCE_DIR/AppDelegate.swift"
    "$SOURCE_DIR/StatusBarController.swift"
    "$SOURCE_DIR/AccessibilityHelper.swift"
    "$SOURCE_DIR/ProcessManager.swift"
    "$SOURCE_DIR/LaunchAtLoginHelper.swift"
    "$SOURCE_DIR/Preferences.swift"
    "$SOURCE_DIR/PreferencesWindow.swift"
)

echo "Compiling Swift sources..."
swiftc \
    -o "$APP_BUNDLE/Contents/MacOS/$APP_NAME" \
    -target arm64-apple-macos12.0 \
    -sdk $(xcrun --show-sdk-path) \
    -framework Cocoa \
    -framework ApplicationServices \
    -framework ServiceManagement \
    -O \
    "${SWIFT_FILES[@]}"

# Also compile for x86_64 and create universal binary (optional)
if [[ "$1" == "--universal" ]]; then
    echo "Creating universal binary..."

    swiftc \
        -o "$APP_BUNDLE/Contents/MacOS/${APP_NAME}_x86" \
        -target x86_64-apple-macos12.0 \
        -sdk $(xcrun --show-sdk-path) \
        -framework Cocoa \
        -framework ApplicationServices \
        -framework ServiceManagement \
        -O \
        "${SWIFT_FILES[@]}"

    # Create universal binary
    lipo -create \
        "$APP_BUNDLE/Contents/MacOS/$APP_NAME" \
        "$APP_BUNDLE/Contents/MacOS/${APP_NAME}_x86" \
        -output "$APP_BUNDLE/Contents/MacOS/${APP_NAME}_universal"

    mv "$APP_BUNDLE/Contents/MacOS/${APP_NAME}_universal" "$APP_BUNDLE/Contents/MacOS/$APP_NAME"
    rm "$APP_BUNDLE/Contents/MacOS/${APP_NAME}_x86"
fi

# Copy the backend into the app bundle (optional - for standalone distribution)
if [[ "$1" == "--bundle-backend" ]] || [[ "$2" == "--bundle-backend" ]]; then
    echo "Bundling backend..."
    BACKEND_DIR="$APP_BUNDLE/Contents/Resources/backend"
    mkdir -p "$BACKEND_DIR"

    # Copy necessary files
    cp -r "$PROJECT_ROOT/dist" "$BACKEND_DIR/"
    cp -r "$PROJECT_ROOT/node_modules" "$BACKEND_DIR/"
    cp "$PROJECT_ROOT/package.json" "$BACKEND_DIR/"
fi

# Code sign (ad-hoc for development)
echo "Code signing..."
codesign --force --deep --sign - "$APP_BUNDLE"

# Optionally sign with a developer certificate
if [[ -n "$CODESIGN_IDENTITY" ]]; then
    echo "Signing with identity: $CODESIGN_IDENTITY"
    codesign --force --deep --sign "$CODESIGN_IDENTITY" \
        --entitlements "$SOURCE_DIR/Konflikt.entitlements" \
        "$APP_BUNDLE"
fi

echo ""
echo "Build complete: $APP_BUNDLE"
echo ""
echo "To run:"
echo "  open $APP_BUNDLE"
echo ""
echo "To install to Applications:"
echo "  cp -r $APP_BUNDLE /Applications/"
