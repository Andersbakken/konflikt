#ifdef __APPLE__

#include "konflikt_native.hpp"
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <thread>
#include <atomic>
#include <mutex>

namespace konflikt {

class MacOSHook : public IPlatformHook {
public:
    MacOSHook() = default;
    ~MacOSHook() override = default;

    bool initialize() override {
        event_tap_ = nullptr;
        run_loop_source_ = nullptr;
        event_loop_ = nullptr;
        is_running_ = false;
        return true;
    }

    void shutdown() override {
        stop_listening();
    }

    State get_state() const override {
        State state{};

        // Get mouse position
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint point = CGEventGetLocation(event);
        CFRelease(event);

        state.x = static_cast<int32_t>(point.x);
        state.y = static_cast<int32_t>(point.y);

        // Get mouse button state
        if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft)) {
            state.mouse_buttons |= ToUInt32(MouseButton::Left);
        }
        if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight)) {
            state.mouse_buttons |= ToUInt32(MouseButton::Right);
        }
        if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonCenter)) {
            state.mouse_buttons |= ToUInt32(MouseButton::Middle);
        }

        // Get keyboard modifiers
        CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);

        if (flags & kCGEventFlagMaskShift) {
            // Note: Can't distinguish left/right shift easily
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftShift);
        }
        if (flags & kCGEventFlagMaskControl) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftControl);
        }
        if (flags & kCGEventFlagMaskAlternate) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftAlt);
        }
        if (flags & kCGEventFlagMaskCommand) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::LeftSuper);
        }
        if (flags & kCGEventFlagMaskAlphaShift) {
            state.keyboard_modifiers |= ToUInt32(KeyboardModifier::CapsLock);
        }

        return state;
    }

    Desktop get_desktop() const override {
        Desktop desktop{};
        CGDirectDisplayID display = CGMainDisplayID();
        desktop.width = static_cast<int32_t>(CGDisplayPixelsWide(display));
        desktop.height = static_cast<int32_t>(CGDisplayPixelsHigh(display));
        return desktop;
    }

    void send_mouse_event(const Event& event) override {
        CGPoint pos = CGPointMake(event.state.x, event.state.y);
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

        CGEventRef cg_event = nullptr;

        switch (event.type) {
            case EventType::MouseMove: {
                cg_event = CGEventCreateMouseEvent(source, kCGEventMouseMoved, pos, kCGMouseButtonLeft);
                break;
            }
            case EventType::MousePress: {
                CGEventType type = kCGEventLeftMouseDown;
                CGMouseButton button = kCGMouseButtonLeft;

                if (event.button == MouseButton::Right) {
                    type = kCGEventRightMouseDown;
                    button = kCGMouseButtonRight;
                } else if (event.button == MouseButton::Middle) {
                    type = kCGEventOtherMouseDown;
                    button = kCGMouseButtonCenter;
                }

                cg_event = CGEventCreateMouseEvent(source, type, pos, button);
                break;
            }
            case EventType::MouseRelease: {
                CGEventType type = kCGEventLeftMouseUp;
                CGMouseButton button = kCGMouseButtonLeft;

                if (event.button == MouseButton::Right) {
                    type = kCGEventRightMouseUp;
                    button = kCGMouseButtonRight;
                } else if (event.button == MouseButton::Middle) {
                    type = kCGEventOtherMouseUp;
                    button = kCGMouseButtonCenter;
                }

                cg_event = CGEventCreateMouseEvent(source, type, pos, button);
                break;
            }
            default:
                break;
        }

        if (cg_event) {
            CGEventPost(kCGHIDEventTap, cg_event);
            CFRelease(cg_event);
        }

        CFRelease(source);
    }

    void send_key_event(const Event& event) override {
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

        bool is_down = (event.type == EventType::KeyPress);
        CGEventRef cg_event = CGEventCreateKeyboardEvent(source, event.keycode, is_down);

        if (cg_event) {
            CGEventPost(kCGHIDEventTap, cg_event);
            CFRelease(cg_event);
        }

        CFRelease(source);
    }

    void start_listening() override {
        if (is_running_) {
            return;
        }

        is_running_ = true;

        // Start event tap in a separate thread
        listener_thread_ = std::thread([this]() {
            RunEventLoop();
        });
    }

    void stop_listening() override {
        if (!is_running_) {
            return;
        }

        is_running_ = false;

        if (event_loop_) {
            CFRunLoopStop(event_loop_);
        }

        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }

        if (event_tap_) {
            CGEventTapEnable(event_tap_, false);
            CFRelease(event_tap_);
            event_tap_ = nullptr;
        }

        if (run_loop_source_) {
            CFRelease(run_loop_source_);
            run_loop_source_ = nullptr;
        }
    }

private:
    static CGEventRef EventTapCallback(
        CGEventTapProxy proxy,
        CGEventType type,
        CGEventRef cg_event,
        void* user_info
    ) {
        auto* hook = static_cast<MacOSHook*>(user_info);

        if (!hook->event_callback) {
            return cg_event;
        }

        Event event{};
        event.timestamp = GetTimestamp();

        // Get current state
        event.state = hook->get_state();

        // Update position from event
        CGPoint point = CGEventGetLocation(cg_event);
        event.state.x = static_cast<int32_t>(point.x);
        event.state.y = static_cast<int32_t>(point.y);

        switch (type) {
            case kCGEventMouseMoved:
            case kCGEventLeftMouseDragged:
            case kCGEventRightMouseDragged:
            case kCGEventOtherMouseDragged:
                event.type = EventType::MouseMove;
                hook->event_callback(event);
                break;

            case kCGEventLeftMouseDown:
                event.type = EventType::MousePress;
                event.button = MouseButton::Left;
                hook->event_callback(event);
                break;

            case kCGEventLeftMouseUp:
                event.type = EventType::MouseRelease;
                event.button = MouseButton::Left;
                hook->event_callback(event);
                break;

            case kCGEventRightMouseDown:
                event.type = EventType::MousePress;
                event.button = MouseButton::Right;
                hook->event_callback(event);
                break;

            case kCGEventRightMouseUp:
                event.type = EventType::MouseRelease;
                event.button = MouseButton::Right;
                hook->event_callback(event);
                break;

            case kCGEventOtherMouseDown:
                event.type = EventType::MousePress;
                event.button = MouseButton::Middle;
                hook->event_callback(event);
                break;

            case kCGEventOtherMouseUp:
                event.type = EventType::MouseRelease;
                event.button = MouseButton::Middle;
                hook->event_callback(event);
                break;

            case kCGEventKeyDown: {
                event.type = EventType::KeyPress;
                event.keycode = static_cast<uint32_t>(CGEventGetIntegerValueField(cg_event, kCGKeyboardEventKeycode));

                // Try to get the text representation
                UniChar chars[4];
                UniCharCount length = 0;
                CGEventKeyboardGetUnicodeString(cg_event, 4, &length, chars);
                if (length > 0) {
                    event.text = std::string(reinterpret_cast<char*>(chars), length * sizeof(UniChar));
                }

                hook->event_callback(event);
                break;
            }

            case kCGEventKeyUp: {
                event.type = EventType::KeyRelease;
                event.keycode = static_cast<uint32_t>(CGEventGetIntegerValueField(cg_event, kCGKeyboardEventKeycode));

                // Try to get the text representation
                UniChar chars[4];
                UniCharCount length = 0;
                CGEventKeyboardGetUnicodeString(cg_event, 4, &length, chars);
                if (length > 0) {
                    event.text = std::string(reinterpret_cast<char*>(chars), length * sizeof(UniChar));
                }

                hook->event_callback(event);
                break;
            }

            default:
                break;
        }

        return cg_event;
    }

    void RunEventLoop() {
        CGEventMask event_mask =
            CGEventMaskBit(kCGEventMouseMoved) |
            CGEventMaskBit(kCGEventLeftMouseDown) |
            CGEventMaskBit(kCGEventLeftMouseUp) |
            CGEventMaskBit(kCGEventRightMouseDown) |
            CGEventMaskBit(kCGEventRightMouseUp) |
            CGEventMaskBit(kCGEventOtherMouseDown) |
            CGEventMaskBit(kCGEventOtherMouseUp) |
            CGEventMaskBit(kCGEventLeftMouseDragged) |
            CGEventMaskBit(kCGEventRightMouseDragged) |
            CGEventMaskBit(kCGEventOtherMouseDragged) |
            CGEventMaskBit(kCGEventKeyDown) |
            CGEventMaskBit(kCGEventKeyUp);

        event_tap_ = CGEventTapCreate(
            kCGSessionEventTap,
            kCGHeadInsertEventTap,
            kCGEventTapOptionListenOnly,
            event_mask,
            EventTapCallback,
            this
        );

        if (!event_tap_) {
            is_running_ = false;
            return;
        }

        run_loop_source_ = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap_, 0);
        event_loop_ = CFRunLoopGetCurrent();
        CFRunLoopAddSource(event_loop_, run_loop_source_, kCFRunLoopCommonModes);
        CGEventTapEnable(event_tap_, true);

        CFRunLoopRun();
    }

    CFMachPortRef event_tap_;
    CFRunLoopSourceRef run_loop_source_;
    CFRunLoopRef event_loop_;
    std::thread listener_thread_;
    std::atomic<bool> is_running_;
};

std::unique_ptr<IPlatformHook> CreatePlatformHook() {
    return std::make_unique<MacOSHook>();
}

} // namespace konflikt

#endif // __APPLE__
