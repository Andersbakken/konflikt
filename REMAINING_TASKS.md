# Konflikt Native Rewrite - Remaining Tasks

## Current Status

The native C++ rewrite is largely functional. The Linux server mode works and can serve the React UI. The WebSocket client is implemented, enabling client mode. The macOS app structure exists but needs testing with Xcode.

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

### Platform Implementations
- [x] Linux (X11/XCB) - PlatformLinux.cpp
  - Input capture via XInput2 raw events
  - Input injection via XTest
  - Display enumeration via RandR
  - Cursor show/hide via pointer grab
  - Clipboard text (basic)
- [x] macOS (CoreGraphics) - PlatformMac.mm
  - Input capture via CGEventTap
  - Input injection via CGEventPost
  - Display enumeration via CGDisplay APIs
  - Cursor show/hide via CGDisplayShowCursor/HideCursor
  - Clipboard text (basic)

### Applications
- [x] Linux CLI application (src/app/main.cpp)
- [x] macOS Swift app structure (src/macos/)
  - ObjC++ bridge (KonfliktBridge)
  - Swift menu bar app skeleton

### Documentation
- [x] BUILDING.md - Build instructions
- [x] Case-insensitive filename fix (konflikt.h → KonfliktAll.h)

## Remaining Tasks

### High Priority

#### 1. macOS App Testing & Fixes
- [ ] Test Swift app builds with Xcode
- [ ] Fix any Swift/ObjC++ bridging issues
- [ ] Test menu bar functionality
- [ ] Add proper app icon
- [ ] Code signing for accessibility permissions

#### 2. End-to-End Testing
- [ ] Test Linux server with Linux client
- [ ] Test Linux server with macOS client
- [ ] Test macOS server with Linux client
- [ ] Verify input events are properly forwarded
- [ ] Verify screen transitions work correctly

#### 3. Service Discovery (mDNS)
- [ ] Implement ServiceDiscoveryLinux.cpp using Avahi
  - Register "_konflikt._tcp" service
  - Browse for servers
  - Resolve server addresses
- [ ] Implement ServiceDiscoveryMac.mm using dns-sd
  - Register service via DNSServiceRegister
  - Browse via DNSServiceBrowse
  - Resolve via DNSServiceResolve
- [ ] Add service discovery callbacks to Konflikt class
- [ ] Auto-connect clients to discovered servers

### Medium Priority

#### 4. Clipboard Sync
- [ ] Wire up clipboard sync in Konflikt.cpp
- [ ] Implement clipboard change detection
- [ ] Add clipboard_sync message type to Protocol
- [ ] Handle multi-format clipboard (text, images, files)
- [ ] Test clipboard sync between machines

#### 5. Mouse Wheel / Scroll Events
- [ ] Add scroll event type to Protocol
- [ ] Implement scroll capture in PlatformLinux.cpp
- [ ] Implement scroll capture in PlatformMac.mm
- [ ] Implement scroll injection on both platforms

#### 6. Preferences & Configuration
- [ ] Add config file support (~/.config/konflikt/config.json)
- [ ] Persist layout configuration
- [ ] Add hotkey configuration for manual screen switching
- [ ] macOS: Preferences window in Swift app

### Lower Priority

#### 7. Error Handling & Robustness
- [ ] Add reconnection logic for clients
- [ ] Handle server restart gracefully
- [ ] Add connection timeout handling
- [ ] Improve error messages and logging

#### 8. Performance Optimization
- [ ] Profile input event latency
- [ ] Optimize JSON serialization if needed
- [ ] Consider binary protocol for high-frequency events

#### 9. Additional Features
- [ ] Drag & drop file transfer
- [ ] Screen edge switching configuration (which edges trigger transition)
- [ ] Multi-monitor awareness (per-display screen edges)
- [ ] Lock cursor to screen option

#### 10. Packaging & Distribution
- [ ] Linux: Create .deb package
- [ ] Linux: Create AppImage
- [ ] macOS: Create signed .dmg
- [ ] macOS: Notarization for Gatekeeper

## File Reference

Key files for continuing development:

```
src/libkonflikt/
├── include/konflikt/
│   ├── Konflikt.h          # Main API class
│   ├── Platform.h          # Platform abstraction interface
│   ├── Protocol.h          # All message types
│   └── ...
├── src/
│   ├── Konflikt.cpp        # Core logic (screen transitions, etc.)
│   ├── PlatformLinux.cpp   # Linux input/display handling
│   ├── PlatformMac.mm      # macOS input/display handling
│   ├── WebSocketClient.cpp # Client connection handling
│   ├── WebSocketServer.cpp # Server connection handling
│   └── ...

src/app/main.cpp            # Linux CLI entry point

src/macos/
├── KonfliktBridge/         # ObjC++ bridge to libkonflikt
└── Konflikt/               # Swift app sources
```

## Notes for Continuation

1. **Building**: Run `mkdir build && cd build && cmake .. -G Ninja && ninja`
2. **Testing server**: `./build/bin/konflikt --verbose --ui-dir=dist/ui`
3. **Testing client**: `./build/bin/konflikt --role=client --server=<host> --verbose`
4. **UI must be built separately**: `cd src/webpage && npm install && npm run build`
5. **macOS Swift app**: May need to open in Xcode for full build/debug
