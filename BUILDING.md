# Building Konflikt

Konflikt can be built in two modes:
1. **Native C++ build** (recommended) - No Node.js required
2. **Legacy Node.js build** - Original implementation

## Native Build (Linux)

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake ninja-build \
    libxcb1-dev libxcb-xinput-dev libxcb-xtest0-dev \
    libxcb-xkb-dev libxcb-randr0-dev libxkbcommon-dev \
    libxkbcommon-x11-dev libssl-dev zlib1g-dev \
    libavahi-client-dev  # Optional: for mDNS auto-discovery

# Fedora
sudo dnf install gcc-c++ cmake ninja-build \
    xcb-util-devel libxcb-devel libxkbcommon-devel \
    libxkbcommon-x11-devel openssl-devel zlib-devel \
    avahi-devel  # Optional: for mDNS auto-discovery
```

### Building

```bash
# Clone with submodules
git clone --recursive https://github.com/Andersbakken/konflikt.git
cd konflikt

# If you already cloned without --recursive:
git submodule update --init --recursive

# Configure and build
mkdir build && cd build
cmake .. -G Ninja
ninja

# The executable will be at build/bin/konflikt
```

### Running

```bash
# Run as server (default)
./build/bin/konflikt --verbose --ui-dir=dist/ui

# Run as client
./build/bin/konflikt --role=client --server=hostname --verbose
```

Options:
- `--role=server|client` - Run as server or client (default: server)
- `--server=HOST` - Server hostname (client auto-discovers if not set)
- `--port=PORT` - Port to use (default: 3000)
- `--ui-dir=PATH` - Directory containing UI files
- `--name=NAME` - Display name for this machine
- `--config=PATH` - Path to config file
- `--verbose` - Enable verbose logging

Edge transition options:
- `--no-edge-left/right/top/bottom` - Disable specific edge transitions
- `--lock-cursor` - Lock cursor to current screen

TLS/Security options:
- `--tls` - Enable TLS/WSS for secure connections
- `--tls-cert=PATH` - Path to TLS certificate file (PEM)
- `--tls-key=PATH` - Path to TLS private key file (PEM)
- `--tls-passphrase=PASS` - Passphrase for encrypted key

Key remapping options:
- `--remap-keys=PRESET` - Use preset: `mac-to-linux` or `linux-to-mac`
- `--remap-key=FROM:TO` - Custom key remap (e.g., `55:133`)
- `--log-keycodes` - Log pressed keycodes (for debugging)

Debug options:
- `--debug-api` - Enable debug API endpoint (`/api/log`)

## Native Build (macOS)

### Prerequisites

- Xcode Command Line Tools
- CMake 3.21+

```bash
xcode-select --install
brew install cmake ninja
```

### Building

The macOS build includes both the C++ library and a Swift menu bar application.

**Using Ninja (recommended for CI/release builds):**
```bash
mkdir build && cd build
cmake .. -G Ninja -DBUILD_UI=OFF
ninja
```

**Using Xcode (recommended for development):**
```bash
mkdir xcode && cd xcode
cmake .. -G Xcode -DBUILD_UI=OFF
open Konflikt.xcodeproj
```

**Note:** CMake's Swift/Ninja integration uses a combined compile+link rule,
which causes Swift files to be recompiled whenever C++ library dependencies change.
For faster incremental builds during development, use the Xcode generator.

### Creating a DMG

Create a distributable disk image:

```bash
./packaging/macos/build-dmg.sh
```

This creates `Konflikt-VERSION.dmg` in the `dist/` directory.

**Code Signing (for distribution):**
```bash
# Sign the app bundle
codesign --deep --force --options runtime \
    --entitlements packaging/macos/Konflikt.entitlements \
    --sign "Developer ID Application: Your Name" \
    dist/Konflikt.app

# Notarize for Gatekeeper
xcrun notarytool submit dist/Konflikt-*.dmg \
    --apple-id YOUR_APPLE_ID \
    --team-id YOUR_TEAM_ID \
    --password YOUR_APP_PASSWORD --wait
```

## Building the React UI

The React UI needs to be built separately:

```bash
cd src/webpage
npm install
npm run build
# Output goes to dist/ui/
```

## Legacy Node.js Build

For the original Node.js-based implementation:

```bash
npm install
npm run build
npm start
```

## Packaging

### Linux AppImage

Create a portable AppImage that works on most Linux distributions:

```bash
./packaging/appimage/build-appimage.sh
```

This will create `Konflikt-*.AppImage` in the `dist/` directory.

Requirements: `wget` (to download linuxdeploy if not installed)

### Linux Debian Package

Build a .deb package:

```bash
dpkg-buildpackage -us -uc -b
```

The package will be created in the parent directory.

### Linux Systemd Service

To run Konflikt as a user service:

```bash
# Copy service file
mkdir -p ~/.config/systemd/user
cp packaging/konflikt.service ~/.config/systemd/user/

# Enable and start
systemctl --user enable konflikt
systemctl --user start konflikt
```

## Project Structure

```
konflikt/
├── CMakeLists.txt           # Top-level CMake
├── src/
│   ├── libkonflikt/         # Core C++ library
│   │   ├── include/         # Public headers
│   │   └── src/             # Implementation
│   ├── app/                 # Linux CLI application
│   ├── macos/               # macOS Swift application
│   └── webpage/             # React UI
├── third_party/
│   ├── uWebSockets/         # WebSocket/HTTP library
│   ├── uSockets/            # Low-level networking
│   └── glaze/               # JSON serialization
├── packaging/
│   ├── appimage/            # AppImage build scripts
│   ├── debian/              # Debian package files
│   ├── macos/               # macOS DMG and code signing
│   └── konflikt.service     # Systemd user service
└── dist/
    └── ui/                  # Built React UI
```
