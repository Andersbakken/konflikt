# Konflikt macOS App (Legacy)

> **Note:** This is the legacy Node.js-based macOS application. The current implementation uses native C++ with a Swift menu bar app. See `src/macos/` for the current implementation and `BUILDING.md` for build instructions.

## Current Native Implementation

The native implementation is located at `src/macos/` and includes:

- **Swift Menu Bar App** - Native menu bar application
- **ObjC++ Bridge** - KonfliktBridge for Swift/C++ interop
- **libkonflikt** - Native C++ core library

Build with CMake:
```bash
mkdir build && cd build
cmake .. -G Ninja -DBUILD_UI=OFF
ninja
```

The app will be at `dist/Konflikt.app`.

---

## Legacy Node.js Implementation

The following documentation is for the legacy implementation that required Node.js:

### Features

- **Menu Bar Icon**: Shows connection status at a glance
- **Accessibility Permissions**: Automatically prompts for required permissions
- **Process Management**: Manages the Node.js backend lifecycle
- **Launch at Login**: Option to start automatically when you log in
- **Web Configuration**: Opens the configuration UI in your default browser

### Prerequisites

- macOS 12.0 or later
- Xcode Command Line Tools (`xcode-select --install`)
- Node.js and the built Konflikt backend

### Build Commands

```bash
# Build the app (ARM64 only - for Apple Silicon)
./build.sh

# Build universal binary (ARM64 + x86_64)
./build.sh --universal

# Build with backend bundled (for distribution)
./build.sh --bundle-backend
```
