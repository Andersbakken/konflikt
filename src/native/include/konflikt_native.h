#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <napi.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace konflikt {

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
    uint32_t keyboard_modifiers { 0 };
    uint32_t mouse_buttons { 0 };
    int32_t x { 0 };
    int32_t y { 0 };
};

struct Desktop
{
    int32_t width { 0 };
    int32_t height { 0 };
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

    virtual State get_state() const     = 0;
    virtual Desktop get_desktop() const = 0;

    virtual void send_mouse_event(const Event &event) = 0;
    virtual void send_key_event(const Event &event)   = 0;

    virtual void start_listening() = 0;
    virtual void stop_listening()  = 0;

    std::function<void(const Event &)> event_callback;
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

    // Internal event handling
    void DispatchEvent(const Event &event);
    void HandlePlatformEvent(const Event &event);

    // Platform-specific implementation
    std::unique_ptr<IPlatformHook> platform_hook_;

    // Event listeners storage
    struct ListenerList
    {
        std::vector<Napi::ThreadSafeFunction> listeners;
    };

    std::unordered_map<EventType, ListenerList> listeners_;

    // Track if listening has started
    bool is_listening_ { false };

    // Thread-safe function for dispatching events from native thread
    Napi::ThreadSafeFunction event_dispatcher_;

    // Logger for native code
    Logger logger_;
};

// Factory function for creating platform-specific hooks
std::unique_ptr<IPlatformHook> CreatePlatformHook();

// Helper functions
uint64_t GetTimestamp();

inline uint32_t ToUInt32(MouseButton button)
{
    return static_cast<uint32_t>(button);
}

inline uint32_t ToUInt32(KeyboardModifier modifier)
{
    return static_cast<uint32_t>(modifier);
}

} // namespace konflikt
