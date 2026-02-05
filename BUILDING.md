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
    libxkbcommon-x11-dev libssl-dev zlib1g-dev

# Fedora
sudo dnf install gcc-c++ cmake ninja-build \
    xcb-util-devel libxcb-devel libxkbcommon-devel \
    libxkbcommon-x11-devel openssl-devel zlib-devel
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
- `--server=HOST` - Server hostname (required for client mode)
- `--port=PORT` - Port to use (default: 3000)
- `--ui-dir=PATH` - Directory containing UI files
- `--name=NAME` - Display name for this machine
- `--verbose` - Enable verbose logging

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

```bash
mkdir build && cd build
cmake .. -G Ninja
ninja
```

**Note:** The Swift app may require building with Xcode for full functionality.
You can open the generated Xcode project or use the CMake build.

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
└── dist/
    └── ui/                  # Built React UI
```
