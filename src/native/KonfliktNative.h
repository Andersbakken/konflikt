#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <napi.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace konflikt {

// MIME type to platform type mapping utilities
class MimeTypeMapper
{
public:
    // Convert MIME type to platform-specific type
    static std::string mimeToMacType(const std::string &mimeType);
    static std::string mimeToX11Type(const std::string &mimeType);

    // Convert platform type to MIME type
    static std::string macTypeToMime(const std::string &macType);
    static std::string x11TypeToMime(const std::string &x11Type);

    // Get all supported MIME types for platform
    static std::vector<std::string> getSupportedMimeTypes();
};

enum class ClipboardSelection
{
    Auto,      // Platform-specific default behavior
    Clipboard, // System clipboard (Ctrl+C/V)
    Primary    // Primary selection (X11 mouse selection)
};

// Logger callback interface for C++ native code
struct Logger
{
    std::function<void(const std::string &)> verbose;
    std::function<void(const std::string &)> debug;
    std::function<void(const std::string &)> log;
    std::function<void(const std::string &)> error;
};

enum class MouseButton : uint32_t
{
    None   = 0x0,
    Left   = 0x1,
    Right  = 0x2,
    Middle = 0x4
};

enum class KeyboardModifier : uint32_t
{
    None         = 0x000,
    LeftShift    = 0x001,
    RightShift   = 0x002,
    LeftAlt      = 0x004,
    RightAlt     = 0x008,
    LeftControl  = 0x010,
    RightControl = 0x020,
    LeftSuper    = 0x040,
    RightSuper   = 0x080,
    CapsLock     = 0x100,
    NumLock      = 0x200,
    ScrollLock   = 0x400
};

struct State
{
    uint32_t keyboardModifiers { 0 };
    uint32_t mouseButtons { 0 };
    int32_t x { 0 };
    int32_t y { 0 };
    int32_t dx { 0 }; // Relative X delta since last event
    int32_t dy { 0 }; // Relative Y delta since last event
};

struct Display
{
    uint32_t id { 0 }; // Display identifier
    int32_t x { 0 };   // Position in virtual desktop coordinate space
    int32_t y { 0 };
    int32_t width { 0 }; // Display dimensions
    int32_t height { 0 };
    bool isPrimary { false };
};

struct Desktop
{
    int32_t width { 0 }; // Total virtual desktop bounding box size
    int32_t height { 0 };
    std::vector<Display> displays; // All displays with absolute positions
};

enum class EventType
{
    MouseMove,
    MousePress,
    MouseRelease,
    KeyPress,
    KeyRelease,
    DesktopChanged
};

struct Event
{
    EventType type;
    uint64_t timestamp;
    State state;

    // Mouse button events
    MouseButton button { MouseButton::None };

    // Key events
    uint32_t keycode { 0 };
    std::string text;
};

// Platform-specific implementation interface
class IPlatformHook
{
public:
    virtual ~IPlatformHook() = default;

    virtual bool initialize(const Logger &logger) = 0;
    virtual void shutdown()                       = 0;

    virtual State getState() const     = 0;
    virtual Desktop getDesktop() const = 0;

    virtual void sendMouseEvent(const Event &event) = 0;
    virtual void sendKeyEvent(const Event &event)   = 0;

    virtual void startListening() = 0;
    virtual void stopListening()  = 0;

    virtual void showCursor()            = 0;
    virtual void hideCursor()            = 0;
    virtual bool isCursorVisible() const = 0;

    // Text clipboard methods with optional selection type
    virtual std::string getClipboardText(ClipboardSelection selection = ClipboardSelection::Auto) const             = 0;
    virtual bool setClipboardText(const std::string &text, ClipboardSelection selection = ClipboardSelection::Auto) = 0;

    // Clipboard methods for multiple MIME types with optional selection type
    virtual std::vector<uint8_t> getClipboardData(const std::string &mimeType, ClipboardSelection selection = ClipboardSelection::Auto) const             = 0;
    virtual bool setClipboardData(const std::string &mimeType, const std::vector<uint8_t> &data, ClipboardSelection selection = ClipboardSelection::Auto) = 0;
    virtual std::vector<std::string> getClipboardMimeTypes(ClipboardSelection selection = ClipboardSelection::Auto) const                                 = 0;

    std::function<void(const Event &)> eventCallback;
};

class KonfliktNative : public Napi::ObjectWrap<KonfliktNative>
{
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    explicit KonfliktNative(const Napi::CallbackInfo &info);
    ~KonfliktNative();

private:
    // Property getters
    Napi::Value GetDesktop(const Napi::CallbackInfo &info);
    Napi::Value GetState(const Napi::CallbackInfo &info);

    // Event listener methods
    void On(const Napi::CallbackInfo &info);
    void Off(const Napi::CallbackInfo &info);

    // Event sending methods
    void SendMouseEvent(const Napi::CallbackInfo &info);
    void SendKeyEvent(const Napi::CallbackInfo &info);

    // Cursor control methods
    void showCursor(const Napi::CallbackInfo &info);
    void hideCursor(const Napi::CallbackInfo &info);
    Napi::Value isCursorVisible(const Napi::CallbackInfo &info);

    // Clipboard methods
    Napi::Value getClipboardText(const Napi::CallbackInfo &info);
    void setClipboardText(const Napi::CallbackInfo &info);
    Napi::Value getClipboardData(const Napi::CallbackInfo &info);
    void setClipboardData(const Napi::CallbackInfo &info);
    Napi::Value getClipboardMimeTypes(const Napi::CallbackInfo &info);

    // Internal event handling
    void dispatchEvent(const Event &event);
    void handlePlatformEvent(const Event &event);

    // Platform-specific implementation
    std::unique_ptr<IPlatformHook> mPlatformHook;

    // Event listeners storage
    struct ListenerEntry
    {
        Napi::ThreadSafeFunction tsfn;
        Napi::FunctionReference funcRef;
    };

    struct ListenerList
    {
        std::vector<ListenerEntry> listeners;
    };

    std::unordered_map<EventType, ListenerList> mListeners;

    // Track if listening has started
    bool mIsListening { false };

    // Thread-safe function for dispatching events from native thread
    Napi::ThreadSafeFunction mEventDispatcher;

    // Logger for native code
    Logger mLogger;

    // Thread-safe functions for logger callbacks
    Napi::ThreadSafeFunction mVerboseTsfn;
    Napi::ThreadSafeFunction mDebugTsfn;
    Napi::ThreadSafeFunction mLogTsfn;
    Napi::ThreadSafeFunction mErrorTsfn;
};

// Factory function for creating platform-specific hooks
std::unique_ptr<IPlatformHook> createPlatformHook();

// Helper functions
uint64_t timestamp();

inline uint32_t toUInt32(MouseButton button)
{
    return static_cast<uint32_t>(button);
}

inline uint32_t toUInt32(KeyboardModifier modifier)
{
    return static_cast<uint32_t>(modifier);
}

} // namespace konflikt
