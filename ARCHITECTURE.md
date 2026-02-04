# Konflikt Architecture

This document describes the architecture of Konflikt, a cross-platform KVM (Keyboard, Video, Mouse) switch application that allows sharing input devices across multiple machines.

## Overview

Konflikt uses a **server-client model** where:
- **Server**: Captures input events and manages the screen layout for all connected clients
- **Clients**: Receive layout assignments from the server and execute forwarded input events

## Directory Structure

```
konflikt/
├── src/
│   ├── app/           # Main application TypeScript code
│   ├── native/        # Native C++ addon + TypeScript bindings
│   └── webpage/       # React UI (separate build)
├── dist/
│   ├── app/           # Compiled application JS
│   ├── native/        # Compiled native modules (Debug/Release)
│   └── ui/            # Built React app (served statically)
└── bin/
    └── konflikt       # Symlink to dist/app/index.js
```

## Core Components

### 1. Native Module (`src/native/`)

The native addon provides platform-specific functionality:

- **KonfliktNative.cpp/h**: Core native implementation
- **KonfliktNativeMac.mm**: macOS-specific input capture
- **KonfliktNativeLinux.cpp**: Linux input handling
- **KonfliktNativeX11.cpp**: X11 display server support
- **KonfliktNativeWayland.cpp**: Wayland display server support

TypeScript bindings:
- **native.ts**: Loads the compiled `.node` module
- **createNativeLogger.ts**: Creates logger callbacks for native code
- **KonfliktNative.d.ts**: Type definitions for native interface

### 2. Main Application (`src/app/`)

#### Entry Point
- **index.ts**: CLI entry point, parses args, loads config, starts main
- **main.ts**: Creates and initializes Konflikt instance

#### Core Classes

**Konflikt.ts** - Main application orchestrator
- Manages role (server/client)
- Creates Server, PeerManager, LayoutManager
- Handles input events and message routing
- Coordinates screen transitions

**Server.ts** - HTTP/WebSocket server using Fastify
- Serves REST API at `/api/*`
- Serves React UI at `/ui/`
- Manages WebSocket connections at `/ws`
- Handles service discovery (mDNS)

**LayoutManager.ts** - Server-side screen layout management
- Tracks all screens (server + clients)
- Auto-arranges new clients (places right of existing)
- Calculates adjacency relationships
- Persists layout to `~/.config/konflikt/layout.json`
- Emits `layoutChanged` events

**PeerManager.ts** - Client-side connection management
- Manages WebSocket connections to servers
- Handles handshake and reconnection
- Broadcasts messages to all connected peers

**WebSocketClient.ts** - Individual WebSocket connection
- Handles connection lifecycle
- Manages handshake protocol
- Sends/receives messages

#### Configuration

**Config.ts** - Configuration manager using convict
- Loads from file, CLI args, environment
- Validates and provides typed access

**configSchema.ts** - Configuration schema definition
- Instance settings (id, name, role)
- Network settings (port, host, discovery)
- Screen settings (position, dimensions)
- Input settings (capture, forwarding)

### 3. Message Protocol

#### Registration Flow

1. Client connects via WebSocket
2. Client sends `HandshakeRequest` with screen geometry
3. Server responds with `HandshakeResponse`
4. Client sends `ClientRegistrationMessage`:
   ```typescript
   {
     type: "client_registration",
     instanceId: string,
     displayName: string,
     machineId: string,
     screenWidth: number,
     screenHeight: number
   }
   ```
5. Server registers client in LayoutManager
6. Server sends `LayoutAssignmentMessage`:
   ```typescript
   {
     type: "layout_assignment",
     position: { x: number, y: number },
     adjacency: { left?, right?, top?, bottom?: string },
     fullLayout: ScreenInfo[]
   }
   ```

#### Layout Updates

When layout changes, server broadcasts `LayoutUpdateMessage`:
```typescript
{
  type: "layout_update",
  screens: ScreenInfo[],
  timestamp: number
}
```

#### Input Events

Input events are forwarded as `InputEventMessage`:
```typescript
{
  type: "input_event",
  sourceInstanceId: string,
  eventType: "keyPress" | "keyRelease" | "mouseMove" | etc,
  eventData: { x, y, timestamp, keyboardModifiers, mouseButtons, ... }
}
```

### 4. REST API

**Status & Config** (both roles):
- `GET /api/status` - Instance status (role, uptime, etc)
- `GET /api/config` - Current configuration
- `PUT /api/config` - Update configuration

**Layout** (server only):
- `GET /api/layout` - Get all screens
- `PUT /api/layout` - Update entire layout
- `PATCH /api/layout/:id` - Update single screen position
- `DELETE /api/layout/:id` - Remove offline client

### 5. React UI (`src/webpage/`)

Built with Vite + React, served from `/ui/`.

**Components:**
- **App.tsx**: Router, role detection
- **LayoutEditor.tsx**: Drag-drop canvas (server)
- **LayoutView.tsx**: Read-only layout display (client)
- **ScreenRect.tsx**: Draggable screen rectangle
- **SettingsForm.tsx**: Local config editor
- **StatusBar.tsx**: Connection status

**API Client:**
- **api/client.ts**: Typed fetch wrappers for REST API

## Data Flow

### Server Startup
```
1. Load config
2. Create Konflikt instance
3. Create LayoutManager
4. Start Fastify server
5. Register API routes
6. Serve static UI
7. Advertise via mDNS
8. Register server screen in layout
9. Listen for input events
```

### Client Startup
```
1. Load config
2. Create Konflikt instance
3. Start Fastify server (for local UI)
4. Discover servers via mDNS
5. Connect to server via WebSocket
6. Complete handshake
7. Send ClientRegistration
8. Receive LayoutAssignment
9. Update local screen position
```

### Layout Change
```
1. UI calls PUT /api/layout
2. LayoutManager.updateLayout() called
3. Positions updated, persisted to config
4. "layoutChanged" event emitted
5. LayoutUpdateMessage broadcast to all clients
6. Clients update their local screen positions
```

## Key Types

**ScreenEntry** (server-side):
```typescript
interface ScreenEntry {
  instanceId: string;
  displayName: string;
  machineId: string;
  x: number;
  y: number;
  width: number;
  height: number;
  isServer: boolean;
  online: boolean;
}
```

**ScreenInfo** (shared/transferred):
```typescript
interface ScreenInfo {
  instanceId: string;
  displayName: string;
  x: number;
  y: number;
  width: number;
  height: number;
  isServer: boolean;
  online: boolean;
}
```

**Adjacency**:
```typescript
interface Adjacency {
  left?: string;   // instanceId of adjacent screen
  right?: string;
  top?: string;
  bottom?: string;
}
```

## Build System

**Scripts:**
- `npm run build` - Full build (lint + native + TS + UI)
- `npm run build:ts` - TypeScript compilation only
- `npm run build:ui` - React UI build only
- `npm run build:native:all` - Debug + Release native builds
- `npm run dev:ui` - Vite dev server with hot reload

**Output:**
- TypeScript compiles `src/app/` → `dist/app/`
- TypeScript compiles `src/native/` → `dist/native/`
- Native builds to `dist/native/Debug/` and `dist/native/Release/`
- UI builds to `dist/ui/`

## Service Discovery

Uses mDNS/Bonjour via `bonjour-service`:
- Service type: `_konflikt._tcp`
- TXT records: `version`, `pid`, `started`, `role`
- Clients discover servers automatically
- Duplicate server detection (older server wins)

## Configuration Files

**Runtime config** (searched in order):
1. `--config` CLI argument
2. `./konflikt.config.js`
3. `./konflikt.config.json`
4. `~/.config/konflikt/config.js`
5. `~/.config/konflikt/config.json`

**Layout persistence:**
- `~/.config/konflikt/layout.json`
- Stores client positions (not server)
- Clients reconnect to saved positions

## Future Considerations

1. **WebSocket Updates**: Real-time layout sync in UI via WebSocket
2. **Client mDNS removal**: Clients don't need to advertise (only servers)
3. **Per-machine shortcuts**: Local hotkey configuration
4. **Encryption**: TLS for WebSocket connections
5. **Authentication**: Token-based client authentication
