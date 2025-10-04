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

    bool initialize(const Logger& logger) override {
        mLogger = logger;
        mEventTap = nullptr;
        mRunLoopSource = nullptr;
        mEventLoop = nullptr;
        mIsRunning = false;
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
        if (mIsRunning) {
            return;
        }

        mIsRunning = true;

        // Start event tap in a separate thread
        mListenerThread = std::thread([this]() {
            RunEventLoop();
        });
    }

    void stop_listening() override {
        if (!mIsRunning) {
            return;
        }

        mIsRunning = false;

        if (mEventLoop) {
            CFRunLoopStop(mEventLoop);
        }

        if (mListenerThread.joinable()) {
            mListenerThread.join();
        }

        if (mEventTap) {
            CGEventTapEnable(mEventTap, false);
            CFRelease(mEventTap);
            mEventTap = nullptr;
        }

        if (mRunLoopSource) {
            CFRelease(mRunLoopSource);
            mRunLoopSource = nullptr;
        }
    }

private:
    static CGEventRef EventTapCallback(
        CGEventTapProxy /*proxy*/,
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

        mEventTap = CGEventTapCreate(
            kCGSessionEventTap,
            kCGHeadInsertEventTap,
            kCGEventTapOptionListenOnly,
            event_mask,
            EventTapCallback,
            this
        );

        if (!mEventTap) {
            mIsRunning = false;
            return;
        }

        mRunLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, mEventTap, 0);
        mEventLoop = CFRunLoopGetCurrent();
        CFRunLoopAddSource(mEventLoop, mRunLoopSource, kCFRunLoopCommonModes);
        CGEventTapEnable(mEventTap, true);

        CFRunLoopRun();
    }

    CFMachPortRef mEventTap;
    CFRunLoopSourceRef mRunLoopSource;
    CFRunLoopRef mEventLoop;
    std::thread mListenerThread;
    std::atomic<bool> mIsRunning;
    Logger mLogger;
};

std::unique_ptr<IPlatformHook> CreatePlatformHook() {
    return std::make_unique<MacOSHook>();
}

} // namespace konflikt

#endif // __APPLE__
