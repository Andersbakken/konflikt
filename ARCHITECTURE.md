# Konflikt Architecture

This document describes the architecture of Konflikt, a cross-platform KVM (Keyboard, Video, Mouse) switch application. The native C++ implementation eliminates the Node.js dependency.

## Overview

Konflikt uses a **server-client model** where:

- **Server**: Captures input events, manages screen layout, and forwards input to clients
- **Clients**: Receive layout assignments and execute forwarded input events

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     libkonflikt (C++ Static Library)            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │  Protocol   │  │  WebSocket  │  │    HTTP     │             │
│  │   (Glaze)   │  │   Server    │  │   Server    │             │
│  │             │  │ (uWebSockets)│  │ (uWebSockets)│            │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │  WebSocket  │  │   Layout    │  │   Konflikt  │             │
│  │   Client    │  │   Manager   │  │  (Main API) │             │
│  │  (uSockets) │  │             │  │             │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│                                                                 │
│  ┌─────────────────────────────────────────────────┐           │
│  │              IPlatform Interface                 │           │
│  │  - Input capture & injection                     │           │
│  │  - Display enumeration                           │           │
│  │  - Cursor visibility                             │           │
│  │  - Clipboard access                              │           │
│  └─────────────────────────────────────────────────┘           │
│                                                                 │
└───────────────────────┬─────────────────────┬───────────────────┘
                        │                     │
            ┌───────────▼───────────┐ ┌───────▼───────────┐
            │  PlatformMac.mm       │ │ PlatformLinux.cpp │
            │  (CoreGraphics)       │ │ (X11/XCB)         │
            │                       │ │                   │
            │  - CGEventTap         │ │ - XInput2         │
            │  - CGEventPost        │ │ - XTest           │
            │  - CGDisplay*         │ │ - RandR           │
            │  - NSPasteboard       │ │ - XCB clipboard   │
            └───────────────────────┘ └───────────────────┘
                        │                     │
            ┌───────────▼───────────┐ ┌───────▼───────────┐
            │   macOS App (Swift)   │ │  Linux App (C++)  │
            │                       │ │                   │
            │  ┌─────────────────┐  │ │  ┌─────────────┐  │
            │  │ KonfliktBridge  │  │ │  │   main.cpp  │  │
            │  │   (ObjC++)      │  │ │  │   (CLI)     │  │
            │  └────────┬────────┘  │ │  └─────────────┘  │
            │           │           │ │                   │
            │  ┌────────▼────────┐  │ │                   │
            │  │  Swift App      │  │ │                   │
            │  │  (Menu Bar)     │  │ │                   │
            │  └─────────────────┘  │ │                   │
            └───────────────────────┘ └───────────────────┘
```

## Directory Structure

```
konflikt/
├── CMakeLists.txt                 # Top-level CMake
├── BUILDING.md                    # Build instructions
├── ARCHITECTURE.md                # This file
├── REMAINING_TASKS.md             # Task tracking
│
├── src/
│   ├── libkonflikt/               # Core C++ library
│   │   ├── CMakeLists.txt
│   │   ├── include/konflikt/      # Public headers
│   │   │   ├── Konflikt.h         # Main API class
│   │   │   ├── Platform.h         # Platform abstraction
│   │   │   ├── Protocol.h         # Message definitions
│   │   │   ├── WebSocketServer.h
│   │   │   ├── WebSocketClient.h
│   │   │   ├── HttpServer.h
│   │   │   ├── LayoutManager.h
│   │   │   ├── Rect.h
│   │   │   └── KonfliktAll.h      # Convenience include
│   │   └── src/
│   │       ├── Konflikt.cpp       # Main logic
│   │       ├── Protocol.cpp
│   │       ├── WebSocketServer.cpp
│   │       ├── WebSocketClient.cpp
│   │       ├── HttpServer.cpp
│   │       ├── LayoutManager.cpp
│   │       ├── Rect.cpp
│   │       ├── PlatformLinux.cpp  # Linux implementation
│   │       ├── PlatformMac.mm     # macOS implementation
│   │       ├── ConfigManager.cpp  # Config file loading/saving
│   │       ├── ServiceDiscoveryLinux.cpp  # Avahi mDNS (Linux)
│   │       └── ServiceDiscoveryMac.mm     # Bonjour mDNS (macOS)
│   │
│   ├── app/                       # Linux CLI application
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   │
│   ├── macos/                     # macOS Swift application
│   │   ├── CMakeLists.txt
│   │   ├── Info.plist.in
│   │   ├── KonfliktBridge/        # ObjC++ bridge
│   │   │   ├── KonfliktBridge.h
│   │   │   └── KonfliktBridge.mm
│   │   └── Konflikt/              # Swift sources
│   │       ├── main.swift
│   │       ├── AppDelegate.swift
│   │       ├── StatusBarController.swift
│   │       └── Konflikt-Bridging-Header.h
│   │
│   ├── webpage/                   # React UI (unchanged)
│   │   └── ...
│   │
│   └── native/                    # Legacy native code (reference)
│       └── ...
│
├── third_party/
│   ├── uWebSockets/               # WebSocket/HTTP (submodule)
│   ├── uSockets/                  # Low-level sockets (submodule)
│   └── glaze/                     # JSON serialization (submodule)
│
├── build/                         # CMake build directory
│   └── bin/konflikt               # Linux executable
│
└── dist/
    └── ui/                        # Built React UI
```

## Core Components

### libkonflikt (C++ Static Library)

The shared library containing all core functionality.

#### Konflikt.h / Konflikt.cpp

Main application class that coordinates everything:

```cpp
class Konflikt {
    bool init();           // Initialize platform, start servers
    void run();            // Main event loop (blocking)
    void stop();           // Clean shutdown
    void quit();           // Signal to stop running

    // Callbacks
    void setStatusCallback(StatusCallback);
    void setLogCallback(LogCallback);
};

struct Config {
    InstanceRole role;     // Server or Client
    std::string instanceId;
    std::string instanceName;
    int port;              // Server port (default 3000)
    std::string serverHost; // For clients
    int serverPort;
    std::string uiPath;    // Path to React UI files
    bool verbose;

    // Edge transition settings
    bool edgeLeft, edgeRight, edgeTop, edgeBottom;
    bool lockCursorToScreen;
    uint32_t lockCursorHotkey;

    // Per-display edge settings
    std::unordered_map<uint32_t, DisplayEdges> displayEdges;

    // Key remapping
    std::unordered_map<uint32_t, uint32_t> keyRemap;

    // TLS/WSS
    bool useTLS;
    std::string tlsCertFile, tlsKeyFile;
};
```

#### Platform.h / PlatformLinux.cpp / PlatformMac.mm

Platform abstraction interface:

```cpp
class IPlatform {
    virtual bool initialize(const Logger &logger) = 0;
    virtual void shutdown() = 0;
    virtual InputState getState() const = 0;
    virtual Desktop getDesktop() const = 0;
    virtual void sendMouseEvent(const Event &event) = 0;
    virtual void sendKeyEvent(const Event &event) = 0;
    virtual void startListening() = 0;
    virtual void stopListening() = 0;
    virtual void showCursor() = 0;
    virtual void hideCursor() = 0;
    virtual bool isCursorVisible() const = 0;
    virtual std::string getClipboardText(...) const = 0;
    virtual bool setClipboardText(...) = 0;

    std::function<void(const Event &)> onEvent;  // Event callback
};
```

**Linux Implementation** (X11/XCB):
- XInput2 for raw input capture
- XTest for input injection
- RandR for display enumeration
- Pointer grab with blank cursor for hiding

**macOS Implementation** (CoreGraphics):
- CGEventTap for input capture
- CGEventPost for input injection
- CGDisplay APIs for display enumeration
- CGDisplayShowCursor/HideCursor for cursor visibility

#### Protocol.h / Protocol.cpp

Message definitions with Glaze JSON serialization:

```cpp
// All messages have a "type" field
struct HandshakeRequest {
    std::string type = "handshake_request";
    std::string instanceId;
    std::string displayName;
    // ...
};

struct InputEventMessage {
    std::string type = "input_event";
    std::string sourceInstanceId;
    std::string eventType;  // "mouseMove", "keyPress", etc.
    InputEventData eventData;
};

// Glaze generates JSON serialization at compile time
template<> struct glz::meta<InputEventMessage> { ... };
```

#### WebSocketServer.h / WebSocketServer.cpp

Server for accepting client connections:

```cpp
class WebSocketServer {
    void start(int port);
    void stop();
    void broadcast(const std::string &message);
    void send(void *connection, const std::string &message);

    // Callbacks
    std::function<void(void *)> onConnect;
    std::function<void(void *, const std::string &)> onMessage;
    std::function<void(void *)> onDisconnect;
};
```

Uses uWebSockets for the server implementation.

#### WebSocketClient.h / WebSocketClient.cpp

Client for connecting to servers:

```cpp
class WebSocketClient {
    bool connect(const std::string &host, int port, const std::string &path);
    void disconnect();
    void send(const std::string &message);
    void poll();

    WebSocketClientCallbacks callbacks;  // onConnect, onMessage, etc.
};
```

Custom implementation using uSockets directly (uWebSockets lacks client support).

#### HttpServer.h / HttpServer.cpp

Static file server for React UI and REST API:

```cpp
class HttpServer {
    void setStaticPath(const std::string &prefix, const std::string &directory);
    void route(const std::string &method, const std::string &path, RouteHandler);
    void start();
    void stop();
};
```

The HTTP server provides a comprehensive REST API:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | Health check (status, version, uptime) |
| `/api/version` | GET | Version info |
| `/api/status` | GET | Instance status, connected clients |
| `/api/config` | GET/POST | Runtime configuration |
| `/api/config/save` | POST | Save config to file |
| `/api/stats` | GET | Input event statistics |
| `/api/keyremap` | GET/POST/DELETE | Key remapping |
| `/api/displays` | GET | Local monitor info |
| `/api/display-edges` | GET/POST/DELETE | Per-display edge settings |
| `/api/layout` | GET | Screen arrangement (server) |
| `/api/servers` | GET | Discovered mDNS servers |
| `/api/connection` | GET | Client connection status |
| `/api/connect` | POST | Connect to server (client) |
| `/api/reconnect` | POST | Force reconnection (client) |
| `/api/disconnect` | POST | Disconnect (client) |

All endpoints support `?pretty` query parameter for formatted JSON output.

#### ServiceDiscovery.h / ServiceDiscoveryMac.mm / ServiceDiscoveryLinux.cpp

mDNS/DNS-SD service discovery for automatic server detection:

```cpp
class ServiceDiscovery {
    bool registerService(const std::string &name, int port, const std::string &instanceId);
    void unregisterService();
    bool startBrowsing();
    void stopBrowsing();
    void poll();
    std::vector<DiscoveredService> getDiscoveredServices() const;

    ServiceDiscoveryCallbacks callbacks;  // onServiceFound, onServiceLost
};
```

- **macOS**: Uses Bonjour (dns_sd.h)
- **Linux**: Uses Avahi (with fallback stub when unavailable)

Servers register themselves as `_konflikt._tcp` services. Clients without a configured server host automatically browse for and connect to discovered servers.

#### LayoutManager.h / LayoutManager.cpp

Manages screen arrangement:

```cpp
class LayoutManager {
    void addScreen(const ScreenInfo &screen);
    void removeScreen(const std::string &instanceId);
    void updateScreenPosition(const std::string &id, int x, int y);
    std::optional<std::string> getAdjacentScreen(const std::string &id, Edge edge);
    std::vector<ScreenInfo> getAllScreens();
};
```

### macOS Application

#### KonfliktBridge (ObjC++)

Objective-C++ wrapper for Swift compatibility:

```objc
@interface KonfliktConfig : NSObject
@property (nonatomic) KonfliktRole role;
@property (nonatomic, copy) NSString *instanceName;
// ...
@end

@interface Konflikt : NSObject
- (instancetype)initWithConfig:(KonfliktConfig *)config;
- (BOOL)initialize;
- (void)run;  // Call on background thread
- (void)stop;
- (void)quit;
@end
```

#### Swift App

Menu bar application:
- `AppDelegate.swift` - App lifecycle, creates Konflikt instance
- `StatusBarController.swift` - Status bar icon and menu

### Linux Application

CLI application (`main.cpp`):

```bash
konflikt [OPTIONS]
  --role=server|client   Run as server or client
  --server=HOST          Server hostname (client mode)
  --port=PORT            Port (default 3000)
  --ui-dir=PATH          React UI directory
  --name=NAME            Display name
  --verbose              Enable verbose logging
```

## Message Protocol

### Message Types

| Message | Direction | Purpose |
|---------|-----------|---------|
| `handshake_request` | Client → Server | Initial connection |
| `handshake_response` | Server → Client | Connection accepted |
| `input_event` | Bidirectional | Mouse/keyboard events |
| `client_registration` | Server → All | New client joined |
| `layout_assignment` | Server → Client | Screen position |
| `layout_update` | Server → All | Layout changed |
| `activate_client` | Server → Client | Switch to this screen |
| `deactivation_request` | Client → Server | Return control |
| `clipboard_sync` | Bidirectional | Clipboard content sync |
| `server_shutdown` | Server → All | Graceful shutdown notice |

### Connection Flow

```
Client                          Server
  |                               |
  |--- WebSocket Connect -------->|
  |                               |
  |--- handshake_request -------->|
  |<-- handshake_response --------|
  |                               |
  |<-- client_registration -------|  (broadcast to all)
  |<-- layout_assignment ---------|
  |                               |
  |<-- input_event ---------------|  (when activated)
  |--- deactivation_request ----->|  (cursor at edge)
```

## Data Flow

### Server Mode

```
┌──────────┐     ┌──────────┐     ┌──────────┐
│ Platform │────▶│ Konflikt │────▶│ WebSocket│────▶ Clients
│  Events  │     │  Logic   │     │  Server  │
└──────────┘     └──────────┘     └──────────┘
                       │
                       ▼
              ┌──────────────┐
              │ HTTP Server  │────▶ React UI
              └──────────────┘
```

### Client Mode

```
┌──────────┐     ┌──────────┐     ┌──────────┐
│ WebSocket│────▶│ Konflikt │────▶│ Platform │
│  Client  │     │  Logic   │     │  Inject  │
└──────────┘     └──────────┘     └──────────┘
```

## Screen Transition Logic

1. Server captures mouse movement via `IPlatform`
2. `Konflikt::checkScreenTransition()` checks if cursor is at screen edge
3. If adjacent client exists:
   - Hide local cursor (`platform->hideCursor()`)
   - Send `activate_client` message to target
   - Forward subsequent `input_event` messages to target
4. Target client:
   - Shows cursor (`platform->showCursor()`)
   - Applies input events (`platform->sendMouseEvent/sendKeyEvent`)
5. When cursor returns to edge:
   - Client sends `deactivation_request`
   - Server shows cursor, resumes local control

## Security

### TLS/WSS Support

Konflikt supports secure WebSocket connections (WSS):

- Server can be configured with TLS certificate and key
- Clients connect via `wss://` when TLS is enabled
- Self-signed certificates are supported (no verification by default)
- Certificate available at `/api/cert` for manual trust

Configuration:
```json
{
  "useTLS": true,
  "tlsCertFile": "/path/to/cert.pem",
  "tlsKeyFile": "/path/to/key.pem"
}
```

CLI: `--tls --tls-cert=cert.pem --tls-key=key.pem`

## Dependencies

| Library | Purpose |
|---------|---------|
| uWebSockets | WebSocket server, HTTP server |
| uSockets | Low-level networking (uWebSockets dep) |
| Glaze | Compile-time JSON serialization (C++23) |
| OpenSSL | SHA256, TLS support |
| XCB/X11 libs | Linux input/display |
| CoreGraphics | macOS input/display |
| Avahi | Linux mDNS service discovery (optional) |

## Build System

CMake-based with Ninja generator:

```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

Options:
- `BUILD_LINUX_APP` - Build Linux CLI (auto on Linux)
- `BUILD_MACOS_APP` - Build macOS Swift app (auto on macOS)
- `BUILD_UI` - Build React UI via npm

Output:
- `build/bin/konflikt` - Linux executable
- `dist/libkonflikt.a` - Static library
- `dist/ui/` - React UI (built separately)

## React UI

The UI is built separately with npm/Vite:

```bash
cd src/webpage
npm install
npm run build  # Output to dist/ui/
```

Served by the HTTP server at `http://localhost:3000/ui/`

Components:
- Layout editor with drag-drop screen arrangement
- Connection status
- Settings panel
