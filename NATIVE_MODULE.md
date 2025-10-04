# Konflikt Native Module

## Overview

The Konflikt native module is a C++20 N-API addon that provides low-level keyboard and mouse event handling for both macOS and Linux (X11).

## Structure

```
native/
├── CMakeLists.txt          # Build configuration
├── include/
│   └── konflikt_native.hpp # Public API and interfaces
└── src/
    ├── konflikt_native.cpp # N-API bindings
    ├── platform_macos.cpp  # macOS implementation
    └── platform_x11.cpp    # Linux/X11 implementation
```

## Building

```bash
npm run build:native    # Build native module only
npm run rebuild:native  # Clean rebuild
npm run build          # Build native + TypeScript
```

The compiled module is output to: `build/Release/konflikt_native.node`

## Usage

### TypeScript

```typescript
import { Konflikt } from "./Konflikt.js";

const k = new Konflikt();

// Access native functionality
console.log(k.native.desktop); // { width: 1920, height: 1080 }
console.log(k.native.state); // { x, y, mouseButtons, keyboardModifiers }

// Listen for events
k.native.on("mouseMove", (event) => {
    console.log("Mouse:", event.x, event.y);
});

k.native.on("keyPress", (event) => {
    console.log("Key:", event.keycode, event.text);
});

// Send events
k.native.sendMouseEvent({
    type: "mousePress",
    button: 0x1, // Left button
    x: 100,
    y: 100,
    timestamp: Date.now(),
    keyboardModifiers: 0,
    mouseButtons: 0
});
```

### JavaScript (Direct)

```javascript
const { KonfliktNative } = require("./build/Release/konflikt_native.node");
const native = new KonfliktNative();

console.log(native.desktop);
console.log(native.state);
```

## API

### Properties

- `desktop`: `KonfliktDesktop` - Get screen dimensions
- `state`: `KonfliktState` - Get current mouse position and modifier states

### Methods

#### Event Listeners

- `on(type, listener)` - Register event listener
- `off(type, listener)` - Remove event listener

**Event types:**

- `"mouseMove"` - Mouse movement
- `"mousePress"` - Mouse button press
- `"mouseRelease"` - Mouse button release
- `"keyPress"` - Keyboard key press
- `"keyRelease"` - Keyboard key release
- `"desktopChanged"` - Desktop resolution changed

#### Event Sending

- `sendMouseEvent(event)` - Send synthetic mouse event
- `sendKeyEvent(event)` - Send synthetic keyboard event

## Platform Support

### macOS

- Uses Core Graphics for event handling
- Uses Carbon for event tapping
- Requires accessibility permissions

### Linux (X11)

- Uses XTest for sending events
- Uses XRecord for listening to events
- Requires X11, XTest, and XRecord extensions

## Types

All types are defined in `src/KonfliktNative.d.ts`:

- `KonfliktMouseButton` - Mouse button enum (Left, Right, Middle)
- `KonfliktKeyboardModifier` - Keyboard modifier flags
- `KonfliktState` - Current input state
- `KonfliktDesktop` - Screen dimensions
- `KonfliktEvent` - Base event interface
- Event-specific interfaces for each event type

## Notes

- Event listening starts automatically when first listener is registered
- Thread-safe event dispatching from native to JavaScript
- Requires appropriate permissions on macOS (accessibility)
- X11 thread support is initialized automatically
