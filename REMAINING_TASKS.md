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
- [ ] Profile input event latency
- [ ] Optimize JSON serialization if needed
- [ ] Consider binary protocol for high-frequency events

#### 8. Additional Features
- [ ] Drag & drop file transfer
- [x] Screen edge switching configuration (edgeLeft/Right/Top/Bottom in config)
- [ ] Multi-monitor awareness (per-display screen edges)
- [x] Lock cursor to screen option (lockCursorToScreen config + setLockCursorToScreen API)

#### 9. Packaging & Distribution
- [x] Linux: Debian packaging files (packaging/debian/)
- [x] Linux: Systemd user service (packaging/konflikt.service)
- [x] Linux: AppImage build script (packaging/appimage/)
- [ ] macOS: Create signed .dmg
- [ ] macOS: Notarization for Gatekeeper

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

Config file location:
- macOS: `~/Library/Application Support/Konflikt/config.json`
- Linux: `~/.config/konflikt/config.json`

Example config:
```json
{
  "role": "server",
  "instanceId": "my-server",
  "instanceName": "My MacBook",
  "port": 3000,
  "serverHost": "",
  "serverPort": 3000,
  "edgeLeft": true,
  "edgeRight": true,
  "edgeTop": true,
  "edgeBottom": true,
  "lockCursorToScreen": false,
  "lockCursorHotkey": 107,
  "verbose": false
}
```

## Notes for Continuation

1. **Building**: `mkdir build && cd build && cmake .. -G Ninja -DBUILD_UI=OFF && ninja`
2. **Testing server**: `./dist/Konflikt.app/Contents/MacOS/Konflikt` (macOS) or `./dist/konflikt` (Linux)
3. **Testing client**: `./dist/konflikt --role=client --server=<host> --verbose`
4. **Auto-discovery**: Clients with no `--server` flag will automatically discover and connect
5. **UI must be built separately**: `cd src/webpage && npm install && npm run build`
6. **macOS Swift app**: Builds with CMake/Ninja, can also open in Xcode for debugging
