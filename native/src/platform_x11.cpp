#ifdef __linux__

#include "konflikt_native.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/record.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>

// X11 defines macros that conflict with our enum names
#undef KeyPress
#undef KeyRelease
#undef None

namespace konflikt {

// Re-define X11 event constants we need
constexpr int X11_KeyPress = 2;
constexpr int X11_KeyRelease = 3;
constexpr int X11_ButtonPress = 4;
constexpr int X11_ButtonRelease = 5;
constexpr int X11_MotionNotify = 6;

class X11Hook : public IPlatformHook {
public:
    X11Hook() = default;
    ~X11Hook() override = default;

    bool initialize() override {
        // Initialize X11 thread support
        if (!XInitThreads()) {
            return false;
        }

        // Open data connection for queries and sending events
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            return false;
        }

        // Open separate connection for XRecord (must be separate!)
        record_display_ = XOpenDisplay(nullptr);
        if (!record_display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
            return false;
        }

        // Check if XRecord extension is available
        int major, minor;
        if (!XRecordQueryVersion(record_display_, &major, &minor)) {
            XCloseDisplay(record_display_);
            XCloseDisplay(display_);
            record_display_ = nullptr;
            display_ = nullptr;
            return false;
        }

        is_running_ = false;
        return true;
    }

    void shutdown() override {
        stop_listening();

        if (display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
        }

        if (record_display_) {
            XCloseDisplay(record_display_);
            record_display_ = nullptr;
        }
    }

    State get_state() const override {
        State state{};

        if (!display_) {
            return state;
        }

        // Get mouse position and button state
        Window root = DefaultRootWindow(display_);
        Window root_return, child_return;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;

        XQueryPointer(display_, root, &root_return, &child_return,
                      &root_x, &root_y, &win_x, &win_y, &mask);

        state.x = root_x;
        state.y = root_y;

        // Parse button mask
        if (mask & Button1Mask) {
            state.mouse_buttons |= ToUInt32(MouseButton::Left);
        }
        if (mask & Button3Mask) {
            state.mouse_buttons |= ToUInt32(MouseButton::Right);
        }
        if (mask & Button2Mask) {
            state.mouse_buttons |= ToUInt32(MouseButton::Middle);
        }

        // Parse modifier mask
        if (mask & ShiftMask) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftShift);
        }
        if (mask & ControlMask) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftControl);
        }
        if (mask & Mod1Mask) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftAlt);
        }
        if (mask & Mod4Mask) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftSuper);
        }
        if (mask & LockMask) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::CapsLock);
        }
        if (mask & Mod2Mask) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::NumLock);
        }

        return state;
    }

    Desktop get_desktop() const override {
        Desktop desktop{};

        if (!display_) {
            return desktop;
        }

        Screen* screen = DefaultScreenOfDisplay(display_);
        desktop.width = WidthOfScreen(screen);
        desktop.height = HeightOfScreen(screen);

        return desktop;
    }

    void send_mouse_event(const Event& event) override {
        if (!display_) {
            return;
        }

        switch (event.type) {
            case EventType::MouseMove:
                XTestFakeMotionEvent(display_, -1, event.state.x, event.state.y, CurrentTime);
                break;

            case EventType::MousePress: {
                unsigned int button = ButtonFromMouseButton(event.button);
                XTestFakeButtonEvent(display_, button, True, CurrentTime);
                break;
            }

            case EventType::MouseRelease: {
                unsigned int button = ButtonFromMouseButton(event.button);
                XTestFakeButtonEvent(display_, button, False, CurrentTime);
                break;
            }

            default:
                break;
        }

        XFlush(display_);
    }

    void send_key_event(const Event& event) override {
        if (!display_) {
            return;
        }

        bool is_press = (event.type == EventType::KeyPress);
        XTestFakeKeyEvent(display_, event.keycode, is_press ? True : False, CurrentTime);
        XFlush(display_);
    }

    void start_listening() override {
        if (is_running_) {
            return;
        }

        is_running_ = true;

        listener_thread_ = std::thread([this]() {
            RunEventLoop();
        });
    }

    void stop_listening() override {
        if (!is_running_) {
            return;
        }

        is_running_ = false;

        // Disable the record context from a different thread to interrupt XRecordEnableContext
        if (record_context_ && record_display_) {
            // Use the data display to disable the context
            XRecordDisableContext(display_, record_context_);
            XFlush(display_);
        }

        // Wait for thread to exit
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }

        // Clean up the context
        if (record_context_ && record_display_) {
            XRecordFreeContext(record_display_, record_context_);
            record_context_ = 0;
        }
    }

private:
    static unsigned int ButtonFromMouseButton(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return Button1;
            case MouseButton::Right: return Button3;
            case MouseButton::Middle: return Button2;
            default: return Button1;
        }
    }

    static MouseButton MouseButtonFromButton(unsigned int button) {
        switch (button) {
            case Button1: return MouseButton::Left;
            case Button3: return MouseButton::Right;
            case Button2: return MouseButton::Middle;
            default: return static_cast<MouseButton>(0);
        }
    }

    static void RecordEventCallback(XPointer closure, XRecordInterceptData* data) {
        auto* hook = reinterpret_cast<X11Hook*>(closure);

        if (!hook->is_running_) {
            XRecordFreeData(data);
            return;
        }

        if (data->category != XRecordFromServer) {
            XRecordFreeData(data);
            return;
        }

        if (!hook->event_callback) {
            XRecordFreeData(data);
            return;
        }

        const auto* event_data = reinterpret_cast<const unsigned char*>(data->data);
        int event_type = event_data[0] & 0x7F;

        Event event{};
        event.timestamp = GetTimestamp();
        event.state = hook->get_state();

        bool should_dispatch = false;

        switch (event_type) {
            case X11_MotionNotify:
                event.type = EventType::MouseMove;
                should_dispatch = true;
                break;

            case X11_ButtonPress: {
                unsigned int button = event_data[1];
                // Filter out scroll wheel events (buttons 4-7)
                if (button >= Button1 && button <= Button3) {
                    event.type = EventType::MousePress;
                    event.button = MouseButtonFromButton(button);
                    should_dispatch = true;
                }
                break;
            }

            case X11_ButtonRelease: {
                unsigned int button = event_data[1];
                // Filter out scroll wheel events (buttons 4-7)
                if (button >= Button1 && button <= Button3) {
                    event.type = EventType::MouseRelease;
                    event.button = MouseButtonFromButton(button);
                    should_dispatch = true;
                }
                break;
            }

            case X11_KeyPress: {
                event.type = EventType::KeyPress;
                event.keycode = event_data[1];

                // Get text representation using XKeysymToString
                KeySym keysym = XkbKeycodeToKeysym(hook->display_, event.keycode, 0, 0);
                if (keysym != NoSymbol) {
                    char* keyname = XKeysymToString(keysym);
                    if (keyname) {
                        // For printable characters, convert to actual character
                        if (keysym >= 0x20 && keysym <= 0x7E) {
                            event.text = std::string(1, static_cast<char>(keysym));
                        } else if (keysym >= 0x100 && keysym <= 0x10FFFF) {
                            // Unicode character
                            event.text = std::string(1, static_cast<char>(keysym & 0xFF));
                        }
                        // For special keys, we could set keyname, but let's leave text empty
                    }
                }

                should_dispatch = true;
                break;
            }

            case X11_KeyRelease: {
                event.type = EventType::KeyRelease;
                event.keycode = event_data[1];

                // Get text representation using XKeysymToString
                KeySym keysym = XkbKeycodeToKeysym(hook->display_, event.keycode, 0, 0);
                if (keysym != NoSymbol) {
                    char* keyname = XKeysymToString(keysym);
                    if (keyname) {
                        // For printable characters, convert to actual character
                        if (keysym >= 0x20 && keysym <= 0x7E) {
                            event.text = std::string(1, static_cast<char>(keysym));
                        } else if (keysym >= 0x100 && keysym <= 0x10FFFF) {
                            // Unicode character
                            event.text = std::string(1, static_cast<char>(keysym & 0xFF));
                        }
                        // For special keys, we could set keyname, but let's leave text empty
                    }
                }

                should_dispatch = true;
                break;
            }

            default:
                break;
        }

        if (should_dispatch) {
            hook->event_callback(event);
        }

        XRecordFreeData(data);
    }

    void RunEventLoop() {
        XRecordClientSpec clients = XRecordAllClients;
        XRecordRange* range = XRecordAllocRange();

        if (!range) {
            fprintf(stderr, "KonfliktNative: Failed to allocate XRecordRange\n");
            is_running_ = false;
            return;
        }

        range->device_events.first = X11_KeyPress;
        range->device_events.last = X11_MotionNotify;

        record_context_ = XRecordCreateContext(record_display_, 0, &clients, 1, &range, 1);

        XFree(range);

        if (!record_context_) {
            fprintf(stderr, "KonfliktNative: Failed to create XRecord context\n");
            is_running_ = false;
            return;
        }

        XSync(record_display_, False);

        fprintf(stderr, "KonfliktNative: Starting XRecord event loop...\n");

        // XRecordEnableContext blocks until XRecordDisableContext is called
        // This is the main event loop - it will only return when we stop listening
        if (!XRecordEnableContext(record_display_, record_context_, RecordEventCallback,
                                  reinterpret_cast<XPointer>(this))) {
            fprintf(stderr, "KonfliktNative: XRecordEnableContext failed or was disabled\n");
            is_running_ = false;
            return;
        }

        // We only get here when XRecordDisableContext is called or there's an error
        // Process any remaining events
        fprintf(stderr, "KonfliktNative: XRecord event loop exited\n");
        XSync(record_display_, False);
        is_running_ = false;
    }

    Display* display_{nullptr};
    Display* record_display_{nullptr};
    XRecordContext record_context_{0};
    std::thread listener_thread_;
    std::atomic<bool> is_running_{false};
};

std::unique_ptr<IPlatformHook> CreatePlatformHook() {
    return std::make_unique<X11Hook>();
}

} // namespace konflikt

#endif // __linux__
