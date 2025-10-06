#ifdef __linux__

#include "KonfliktNative.h"
#include <cstdlib>
#include <cstring>
#include <memory>

// Include X11 implementation (always available on Linux)
#include "KonfliktNativeX11.cpp"

// Include Wayland implementation (only if available)
#ifdef WAYLAND_AVAILABLE
#include "KonfliktNativeWayland.cpp"
#endif

namespace konflikt {

// Platform detection functions
bool isWaylandAvailable()
{
    // Check for Wayland display environment variable
    const char *waylandDisplay = getenv("WAYLAND_DISPLAY");
    if (waylandDisplay && strlen(waylandDisplay) > 0) {
        return true;
    }

    // Check for XDG_SESSION_TYPE
    const char *sessionType = getenv("XDG_SESSION_TYPE");
    if (sessionType && strcmp(sessionType, "wayland") == 0) {
        return true;
    }

    return false;
}

bool isX11Available()
{
    // Check for X11 display environment variable
    const char *x11Display = getenv("DISPLAY");
    if (x11Display && strlen(x11Display) > 0) {
        return true;
    }

    // Check for XDG_SESSION_TYPE
    const char *sessionType = getenv("XDG_SESSION_TYPE");
    if (sessionType && strcmp(sessionType, "x11") == 0) {
        return true;
    }

    return false;
}

// Factory function for creating platform-specific hooks
std::unique_ptr<IPlatformHook> createPlatformHook()
{
#ifdef WAYLAND_AVAILABLE
    // Try Wayland first if available and compiled in
    if (isWaylandAvailable()) {
        try {
            // Create Wayland hook
            auto waylandHook = std::make_unique<WaylandHook>();

            // Test if Wayland connection works
            Logger testLogger = {
                [](const std::string &) {
            }, // verbose - silent for test
                [](const std::string &) {
            }, // debug - silent for test
                [](const std::string &) {
            }, // log - silent for test
                [](const std::string &) {
            } // error - silent for test
            };

            if (waylandHook->initialize(testLogger)) {
                waylandHook->shutdown();                // Clean shutdown after test
                return std::make_unique<WaylandHook>(); // Return fresh instance
            }
        } catch (...) {
            // Wayland initialization failed, fall back to X11
        }
    }
#endif

    // Fall back to X11 if Wayland is not available or failed
    if (isX11Available()) {
        return std::make_unique<XCBHook>();
    }

    // Neither Wayland nor X11 is available
    return nullptr;
}

} // namespace konflikt

#endif // __linux__