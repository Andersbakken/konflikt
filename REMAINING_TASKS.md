# Konflikt Native Rewrite - Remaining Tasks

## Current Status

The native C++ rewrite is largely functional. The macOS Swift app builds and runs with CMake/Ninja. Service discovery, clipboard sync, scroll events, config management, and auto-reconnection are all implemented.

## Completed

### Core Library (libkonflikt)
- [x] CMake build system with uWebSockets/uSockets/Glaze submodules
- [x] Protocol message types with Glaze JSON serialization
- [x] WebSocket server using uWebSockets
- [x] WebSocket client using uSockets (custom implementation)
- [x] HTTP server for React UI static file serving
- [x] Layout manager for screen arrangement
- [x] Rect utility class for geometry calculations
- [x] Platform abstraction interface (IPlatform)
- [x] Service Discovery interface and macOS implementation (dns-sd/Bonjour)
- [x] Service Discovery integration in Konflikt class (auto-register/browse)
- [x] Clipboard sync protocol message and sync logic
- [x] Mouse wheel/scroll events support (macOS)
- [x] ConfigManager for loading/saving JSON config files
- [x] Auto-reconnection logic for clients
- [x] WebSocket heartbeat with ping/pong for dead connection detection
- [x] Graceful server shutdown notification protocol

### Platform Implementations
- [x] Linux (X11/XCB) - PlatformLinux.cpp
  - Input capture via XInput2 raw events (including scroll wheel)
  - Input injection via XTest (including scroll wheel)
  - Display enumeration via RandR
  - Cursor show/hide via pointer grab
  - Clipboard text via xclip/xsel (both primary and clipboard selection)
  - Service discovery via Avahi (when available)
- [x] macOS (CoreGraphics) - PlatformMac.mm
  - Input capture via CGEventTap (including scroll wheel)
  - Input injection via CGEventPost (including scroll wheel)
  - Display enumeration via CGDisplay APIs
  - Cursor show/hide via CGDisplayShowCursor/HideCursor
  - Clipboard text (basic)

### Applications
- [x] Linux CLI application (src/app/main.cpp)
- [x] macOS Swift app (src/macos/)
  - ObjC++ bridge (KonfliktBridge) with ARC
  - Swift menu bar app
  - Builds with CMake/Ninja (fixed bridging header, Info.plist)

### Documentation
- [x] BUILDING.md - Build instructions
- [x] Case-insensitive filename fix (konflikt.h → KonfliktAll.h)

## Remaining Tasks

### High Priority

#### 1. macOS App Polish
- [ ] Add proper app icon
- [ ] Code signing for accessibility permissions
- [ ] Test with Xcode for debugging

#### 2. End-to-End Testing
- [ ] Test Linux server with Linux client
- [ ] Test Linux server with macOS client
- [ ] Test macOS server with Linux client
- [ ] Verify input events are properly forwarded
- [ ] Verify screen transitions work correctly

### Medium Priority

#### 3. Linux Platform Updates
- [x] Implement ServiceDiscoveryLinux.cpp using Avahi (with fallback stub when Avahi not available)
- [x] Implement scroll capture in PlatformLinux.cpp (buttons 4-7 as scroll events)
- [x] Implement scroll injection in PlatformLinux.cpp (via XTest fake button press)

#### 4. Clipboard Sync Enhancements
- [ ] Handle multi-format clipboard (images, files)
- [ ] Test clipboard sync between machines

#### 5. Preferences UI
- [x] macOS: Preferences window in Swift app (edge transitions, cursor lock)
- [x] Hot key for cursor lock toggle (lockCursorHotkey config, default: Scroll Lock)

### Lower Priority

#### 6. Error Handling Improvements
- [x] Auto-reconnection logic for clients (DONE)
- [x] Connection timeout handling (10 second timeout during handshake)
- [x] WebSocket heartbeat/ping-pong (30s interval, 10s timeout)
- [x] Handle server restart gracefully (server_shutdown message, faster reconnection)
- [x] Improved logging with timestamps

#### 7. Performance Optimization
- [x] Profile input event latency (latency tracking in /api/stats)
- [ ] Optimize JSON serialization if needed
- [ ] Consider binary protocol for high-frequency events

#### 8. Additional Features
- [ ] Drag & drop file transfer
- [x] Screen edge switching configuration (edgeLeft/Right/Top/Bottom in config)
- [ ] Multi-monitor awareness (per-display screen edges)
- [x] Lock cursor to screen option (lockCursorToScreen config + setLockCursorToScreen API)

#### 10. Security (Optional)
- [x] WSS (WebSocket Secure) server-side support
  - TLS config options: useTLS, tlsCertFile, tlsKeyFile, tlsKeyPassphrase
  - CLI options: --tls, --tls-cert, --tls-key, --tls-passphrase
  - Certificate generation script: scripts/generate-cert.sh
  - HTTP endpoint /api/cert for clients to download server certificate
  - HTTP endpoint /api/server-info returns server TLS status
- [x] WSS client-side support (connect to wss:// servers)
  - Client uses TLS when useTLS config is enabled
  - Self-signed certs supported (no verification by default)
- [ ] Config file signing or encryption (optional)
  - Prevent tampering with config files
  - Could use simple HMAC or full encryption
- [ ] Note: HTTP for config UI is acceptable (browsers won't trust self-signed certs anyway)

#### 9. Packaging & Distribution
- [x] Linux: Debian packaging files (packaging/debian/)
- [x] Linux: Systemd user service (packaging/konflikt.service)
- [x] Linux: AppImage build script (packaging/appimage/)
- [x] macOS: DMG build script (packaging/macos/build-dmg.sh)
- [x] macOS: Entitlements file for code signing
- [ ] macOS: Notarization for Gatekeeper (requires Apple Developer account)

## File Reference

Key files for continuing development:

```
src/libkonflikt/
├── include/konflikt/
│   ├── ConfigManager.h     # Config file load/save
│   ├── Konflikt.h          # Main API class
│   ├── Platform.h          # Platform abstraction interface
│   ├── Protocol.h          # All message types
│   ├── ServiceDiscovery.h  # mDNS service discovery
│   └── ...
├── src/
│   ├── ConfigManager.cpp   # Config file implementation
│   ├── Konflikt.cpp        # Core logic (screen transitions, clipboard, reconnect, etc.)
│   ├── PlatformLinux.cpp   # Linux input/display handling
│   ├── PlatformMac.mm      # macOS input/display handling
│   ├── ServiceDiscoveryMac.mm   # macOS Bonjour implementation
│   ├── ServiceDiscoveryLinux.cpp # Linux Avahi implementation
│   ├── WebSocketClient.cpp # Client connection handling (with reconnect)
│   ├── WebSocketServer.cpp # Server connection handling
│   └── ...

src/app/main.cpp            # Linux CLI entry point

src/macos/
├── KonfliktBridge/         # ObjC++ bridge to libkonflikt
└── Konflikt/               # Swift app sources
```

## Config File Format

Config file locations (searched in order, first found wins):

**User config (takes precedence):**
- macOS: `~/Library/Application Support/Konflikt/config.json`
- Linux: `$XDG_CONFIG_HOME/konflikt/config.json` (default: `~/.config/konflikt/`)

**System config (fallback):**
- macOS: `/Library/Application Support/Konflikt/config.json`
- Linux: `$XDG_CONFIG_DIRS/konflikt/config.json` (default: `/etc/xdg/konflikt/`)

Example config (all options):
```json
{
  "role": "server",
  "instanceId": "my-server",
  "instanceName": "My MacBook",
  "port": 3000,
  "serverHost": "",
  "serverPort": 3000,
  "screenX": 0,
  "screenY": 0,
  "screenWidth": 0,
  "screenHeight": 0,
  "edgeLeft": true,
  "edgeRight": true,
  "edgeTop": true,
  "edgeBottom": true,
  "lockCursorToScreen": false,
  "lockCursorHotkey": 107,
  "uiPath": "",
  "useTLS": false,
  "tlsCertFile": "",
  "tlsKeyFile": "",
  "tlsKeyPassphrase": "",
  "verbose": false,
  "logFile": "",
  "enableDebugApi": false,
  "logKeycodes": false,
  "keyRemap": {
    "55": 133,
    "54": 134,
    "58": 64,
    "61": 108
  }
}
```

### Key Remapping

Cross-platform modifier key remapping is supported. Common keycodes:

| Key | macOS | Linux |
|-----|-------|-------|
| Command/Super Left | 55 | 133 |
| Command/Super Right | 54 | 134 |
| Option/Alt Left | 58 | 64 |
| Option/Alt Right | 61 | 108 |

CLI presets:
- `--remap-keys=mac-to-linux` - Running on Mac, controlling Linux
- `--remap-keys=linux-to-mac` - Running on Linux, controlling Mac
- `--remap-key=55:133` - Custom single key remap

## HTTP API Endpoints

All endpoints support `?pretty` query parameter for formatted JSON output.

- `GET /health` - Health check endpoint (status, version, uptime in ms)
- `GET /api/version` - Version info
- `GET /api/server-info` - Server name, port, TLS status
- `GET /api/status` - Instance status, connection info, client details
- `GET /api/config` - Get current runtime configuration
- `POST /api/config` - Update runtime config (JSON body with edgeLeft, edgeRight, edgeTop, edgeBottom, lockCursorToScreen, verbose, logKeycodes)
- `POST /api/config/save` - Save current config to file
- `GET /api/stats` - Input event statistics (totalEvents, mouseEvents, keyEvents, scrollEvents, eventsPerSecond, latency: lastMs/avgMs/maxMs/samples)
- `POST /api/stats/reset` - Reset all statistics counters
- `POST /api/keyremap` - Add key remap: `{"from": 55, "to": 133}` or use preset: `{"preset": "mac-to-linux"}`, `{"preset": "linux-to-mac"}`, `{"preset": "clear"}`
- `DELETE /api/keyremap` - Remove key remap: `{"from": 55}`
- `GET /api/cert` - Download server TLS certificate (if TLS enabled)
- `GET /api/log` - Recent logs with key data filtered (if `enableDebugApi` enabled)

## Notes for Continuation

1. **Building**: `mkdir build && cd build && cmake .. -G Ninja -DBUILD_UI=OFF && ninja`
2. **Testing server**: `./dist/Konflikt.app/Contents/MacOS/Konflikt` (macOS) or `./dist/konflikt` (Linux)
3. **Testing client**: `./dist/konflikt --role=client --server=<host> --verbose`
4. **Auto-discovery**: Clients with no `--server` flag will automatically discover and connect
5. **UI must be built separately**: `cd src/webpage && npm install && npm run build`
6. **macOS Swift app**: Builds with CMake/Ninja, can also open in Xcode for debugging
