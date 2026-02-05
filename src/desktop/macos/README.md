# Konflikt macOS App

A native macOS menu bar application for Konflikt.

## Features

- **Menu Bar Icon**: Shows connection status at a glance
- **Accessibility Permissions**: Automatically prompts for required permissions
- **Process Management**: Manages the Node.js backend lifecycle
- **Launch at Login**: Option to start automatically when you log in
- **Web Configuration**: Opens the configuration UI in your default browser

## Building

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

### Output

The built app will be at: `dist/desktop/macos/Konflikt.app`

## Installation

```bash
# Copy to Applications folder
cp -r dist/desktop/macos/Konflikt.app /Applications/

# Or open directly from dist
open dist/desktop/macos/Konflikt.app
```

## Configuration

The app will look for the Konflikt backend in the following locations:

1. Inside the app bundle (`Konflikt.app/Contents/Resources/backend/`)
2. Development path relative to the app
3. `~/dev/konflikt/dist/app/index.js`
4. `/usr/local/share/konflikt/dist/app/index.js`
5. `/opt/konflikt/dist/app/index.js`

## Code Signing

For development, the app is signed with an ad-hoc signature. For distribution:

```bash
# Sign with your Developer ID
export CODESIGN_IDENTITY="Developer ID Application: Your Name (XXXXXXXXXX)"
./build.sh
```

## Accessibility Permissions

The app requires Accessibility permissions to control the mouse and keyboard. On first launch, it will prompt you to grant access:

1. Open System Preferences → Security & Privacy → Privacy → Accessibility
2. Click the lock to make changes
3. Add Konflikt to the list and enable it

## Architecture

```
Konflikt.app/
├── Contents/
│   ├── Info.plist          # App metadata
│   ├── MacOS/
│   │   └── Konflikt        # Native Swift executable
│   └── Resources/
│       └── backend/        # (Optional) Bundled Node.js backend
```

The Swift app:
- Displays a menu bar icon with status
- Spawns and manages the Node.js backend process
- Monitors backend output for connection status
- Handles accessibility permission requests
- Provides "Start at Login" functionality
