#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace konflikt {

// Forward declarations
struct Event;

/// Mouse button flags
enum class MouseButton : uint32_t
{
    None = 0x0,
    Left = 0x1,
    Right = 0x2,
    Middle = 0x4
};

/// Keyboard modifier flags
enum class KeyboardModifier : uint32_t
{
    None = 0x000,
    LeftShift = 0x001,
    RightShift = 0x002,
    LeftAlt = 0x004,
    RightAlt = 0x008,
    LeftControl = 0x010,
    RightControl = 0x020,
    LeftSuper = 0x040,
    RightSuper = 0x080,
    CapsLock = 0x100,
    NumLock = 0x200,
    ScrollLock = 0x400
};

/// Input state snapshot
struct InputState
{
    int32_t x {};
    int32_t y {};
    int32_t dx {};
    int32_t dy {};
    double scrollX {};  // Horizontal scroll delta
    double scrollY {};  // Vertical scroll delta
    uint32_t keyboardModifiers {};
    uint32_t mouseButtons {};
};

/// Display information
struct Display
{
    uint32_t id {};
    int32_t x {};
    int32_t y {};
    int32_t width {};
    int32_t height {};
    bool isPrimary { false };
};

/// Desktop (virtual screen) information
struct Desktop
{
    int32_t width {};
    int32_t height {};
    std::vector<Display> displays;
};

/// Event types
enum class EventType
{
    MouseMove,
    MousePress,
    MouseRelease,
    MouseScroll,
    KeyPress,
    KeyRelease,
    DesktopChanged
};

/// Input/desktop event
struct Event
{
    EventType type;
    uint64_t timestamp {};
    InputState state;
    MouseButton button { MouseButton::None };
    uint32_t keycode {};
    std::string text;
};

/// Clipboard selection type
enum class ClipboardSelection
{
    Auto,
    Clipboard,
    Primary
};

/// Logger interface
struct Logger
{
    std::function<void(const std::string &)> verbose;
    std::function<void(const std::string &)> debug;
    std::function<void(const std::string &)> log;
    std::function<void(const std::string &)> error;
};

/// Platform abstraction interface
class IPlatform
{
public:
    virtual ~IPlatform() = default;

    /// Initialize the platform hook
    virtual bool initialize(const Logger &logger) = 0;

    /// Shutdown and cleanup
    virtual void shutdown() = 0;

    /// Get current input state
    virtual InputState getState() const = 0;

    /// Get desktop information
    virtual Desktop getDesktop() const = 0;

    /// Send a mouse event
    virtual void sendMouseEvent(const Event &event) = 0;

    /// Send a keyboard event
    virtual void sendKeyEvent(const Event &event) = 0;

    /// Start listening for input events
    virtual void startListening() = 0;

    /// Stop listening for input events
    virtual void stopListening() = 0;

    /// Show the cursor
    virtual void showCursor() = 0;

    /// Hide the cursor
    virtual void hideCursor() = 0;

    /// Check if cursor is visible
    virtual bool isCursorVisible() const = 0;

    /// Get clipboard text
    virtual std::string getClipboardText(ClipboardSelection selection = ClipboardSelection::Auto) const = 0;

    /// Set clipboard text
    virtual bool setClipboardText(const std::string &text, ClipboardSelection selection = ClipboardSelection::Auto) = 0;

    /// Event callback
    std::function<void(const Event &)> onEvent;
};

/// Create platform-specific implementation
std::unique_ptr<IPlatform> createPlatform();

/// Helper functions
inline uint32_t toUInt32(MouseButton button)
{
    return static_cast<uint32_t>(button);
}

inline uint32_t toUInt32(KeyboardModifier modifier)
{
    return static_cast<uint32_t>(modifier);
}

/// Get current timestamp in milliseconds
inline uint64_t timestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

} // namespace konflikt
