// macOS platform implementation

#ifdef __APPLE__

#include "konflikt/Platform.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace konflikt {

class MacOSPlatform : public IPlatform
{
public:
    MacOSPlatform() = default;

    ~MacOSPlatform() override { shutdown(); }

    bool initialize(const Logger &logger) override
    {
        mLogger = logger;
        mEventTap = nullptr;
        mRunLoopSource = nullptr;
        mEventLoop = nullptr;
        mIsRunning = false;
        mCursorVisible = true;

        // Initialize current desktop state with all displays
        updateDesktopInfo();

        // Register for display configuration changes
        CGDisplayRegisterReconfigurationCallback(displayConfigurationCallback, this);

        return true;
    }

    void shutdown() override
    {
        stopListening();

        // Unregister display configuration callback
        CGDisplayRemoveReconfigurationCallback(displayConfigurationCallback, this);
    }

    InputState getState() const override
    {
        InputState state {};

        // Get mouse position
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint point = CGEventGetLocation(event);
        CFRelease(event);

        state.x = static_cast<int32_t>(point.x);
        state.y = static_cast<int32_t>(point.y);

        // Get mouse button state
        if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft)) {
            state.mouseButtons |= toUInt32(MouseButton::Left);
        }
        if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight)) {
            state.mouseButtons |= toUInt32(MouseButton::Right);
        }
        if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonCenter)) {
            state.mouseButtons |= toUInt32(MouseButton::Middle);
        }

        // Get keyboard modifiers
        CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);

        if (flags & kCGEventFlagMaskShift) {
            state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftShift);
        }
        if (flags & kCGEventFlagMaskControl) {
            state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftControl);
        }
        if (flags & kCGEventFlagMaskAlternate) {
            state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftAlt);
        }
        if (flags & kCGEventFlagMaskCommand) {
            state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftSuper);
        }
        if (flags & kCGEventFlagMaskAlphaShift) {
            state.keyboardModifiers |= toUInt32(KeyboardModifier::CapsLock);
        }

        return state;
    }

    Desktop getDesktop() const override
    {
        std::lock_guard<std::mutex> lock(mDesktopMutex);
        return mCurrentDesktop;
    }

    void sendMouseEvent(const Event &event) override
    {
        CGPoint pos = CGPointMake(event.state.x, event.state.y);
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

        CGEventRef cgEvent = nullptr;

        switch (event.type) {
            case EventType::MouseMove: {
                // Use CGWarpMouseCursorPosition to actually move the cursor
                CGWarpMouseCursorPosition(pos);

                // Determine event type based on mouse button state (for dragging)
                CGEventType moveType = kCGEventMouseMoved;
                CGMouseButton button = kCGMouseButtonLeft;
                uint32_t mouseButtons = event.state.mouseButtons;

                if (mouseButtons & toUInt32(MouseButton::Left)) {
                    moveType = kCGEventLeftMouseDragged;
                    button = kCGMouseButtonLeft;
                } else if (mouseButtons & toUInt32(MouseButton::Right)) {
                    moveType = kCGEventRightMouseDragged;
                    button = kCGMouseButtonRight;
                } else if (mouseButtons & toUInt32(MouseButton::Middle)) {
                    moveType = kCGEventOtherMouseDragged;
                    button = kCGMouseButtonCenter;
                }

                cgEvent = CGEventCreateMouseEvent(source, moveType, pos, button);
                break;
            }
            case EventType::MousePress: {
                // Warp cursor to position first to ensure click happens at right location
                CGWarpMouseCursorPosition(pos);

                CGEventType type = kCGEventLeftMouseDown;
                CGMouseButton button = kCGMouseButtonLeft;

                if (event.button == MouseButton::Right) {
                    type = kCGEventRightMouseDown;
                    button = kCGMouseButtonRight;
                } else if (event.button == MouseButton::Middle) {
                    type = kCGEventOtherMouseDown;
                    button = kCGMouseButtonCenter;
                }

                cgEvent = CGEventCreateMouseEvent(source, type, pos, button);
                break;
            }
            case EventType::MouseRelease: {
                // Warp cursor to position first to ensure release happens at right location
                CGWarpMouseCursorPosition(pos);

                CGEventType type = kCGEventLeftMouseUp;
                CGMouseButton button = kCGMouseButtonLeft;

                if (event.button == MouseButton::Right) {
                    type = kCGEventRightMouseUp;
                    button = kCGMouseButtonRight;
                } else if (event.button == MouseButton::Middle) {
                    type = kCGEventOtherMouseUp;
                    button = kCGMouseButtonCenter;
                }

                cgEvent = CGEventCreateMouseEvent(source, type, pos, button);
                break;
            }
            case EventType::MouseScroll: {
                // Create scroll wheel event
                // wheelCount=2 for both axes, units are in "lines"
                cgEvent = CGEventCreateScrollWheelEvent(
                    source, kCGScrollEventUnitPixel, 2,
                    static_cast<int32_t>(event.state.scrollY),
                    static_cast<int32_t>(event.state.scrollX));
                break;
            }
            default:
                break;
        }

        if (cgEvent) {
            CGEventPost(kCGHIDEventTap, cgEvent);
            CFRelease(cgEvent);
        }

        CFRelease(source);
    }

    void sendKeyEvent(const Event &event) override
    {
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

        bool isDown = (event.type == EventType::KeyPress);
        CGEventRef cgEvent = CGEventCreateKeyboardEvent(source, event.keycode, isDown);

        if (cgEvent) {
            CGEventPost(kCGHIDEventTap, cgEvent);
            CFRelease(cgEvent);
        }

        CFRelease(source);
    }

    void startListening() override
    {
        if (mIsRunning) {
            return;
        }

        mIsRunning = true;

        // Start event tap in a separate thread
        mListenerThread = std::thread([this]() {
            runEventLoop();
        });
    }

    void stopListening() override
    {
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

    void showCursor() override
    {
        if (mCursorVisible) {
            return;
        }

        CGDisplayShowCursor(kCGDirectMainDisplay);
        mCursorVisible = true;
    }

    void hideCursor() override
    {
        if (!mCursorVisible) {
            return;
        }

        CGDisplayHideCursor(kCGDirectMainDisplay);
        mCursorVisible = false;
    }

    bool isCursorVisible() const override { return mCursorVisible; }

    std::string getClipboardText(ClipboardSelection /*selection*/) const override
    {
        // macOS only has system clipboard, ignore Primary selection
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSString *string = [pasteboard stringForType:NSPasteboardTypeString];

        if (string) {
            return std::string([string UTF8String]);
        }

        return "";
    }

    bool setClipboardText(const std::string &text, ClipboardSelection /*selection*/) override
    {
        // macOS only has system clipboard, ignore Primary selection
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];

        NSString *nsString = [NSString stringWithUTF8String:text.c_str()];
        BOOL success = [pasteboard setString:nsString forType:NSPasteboardTypeString];

        return success == YES;
    }

private:
    static void displayConfigurationCallback(
        CGDirectDisplayID /*display*/,
        CGDisplayChangeSummaryFlags flags,
        void *userInfo)
    {
        auto *platform = static_cast<MacOSPlatform *>(userInfo);

        // Only respond to configuration changes (not just enable/disable)
        if (flags & kCGDisplayDesktopShapeChangedFlag) {
            platform->onDisplayConfigurationChanged();
        }
    }

    void updateDesktopInfo()
    {
        Desktop newDesktop;

        // Get all active displays
        uint32_t maxDisplays = 32;
        CGDirectDisplayID displays[32];
        uint32_t displayCount = 0;

        if (CGGetActiveDisplayList(maxDisplays, displays, &displayCount) != kCGErrorSuccess) {
            mLogger.error("Failed to get active display list");
            return;
        }

        CGDirectDisplayID mainDisplay = CGMainDisplayID();

        // Calculate bounding box and collect display info
        int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
        bool first = true;

        for (uint32_t i = 0; i < displayCount; i++) {
            CGRect bounds = CGDisplayBounds(displays[i]);

            Display display;
            display.id = static_cast<uint32_t>(displays[i]);
            display.x = static_cast<int32_t>(bounds.origin.x);
            display.y = static_cast<int32_t>(bounds.origin.y);
            display.width = static_cast<int32_t>(bounds.size.width);
            display.height = static_cast<int32_t>(bounds.size.height);
            display.isPrimary = (displays[i] == mainDisplay);

            newDesktop.displays.push_back(display);

            // Update bounding box
            if (first) {
                minX = display.x;
                minY = display.y;
                maxX = display.x + display.width;
                maxY = display.y + display.height;
                first = false;
            } else {
                minX = std::min(minX, display.x);
                minY = std::min(minY, display.y);
                maxX = std::max(maxX, display.x + display.width);
                maxY = std::max(maxY, display.y + display.height);
            }
        }

        newDesktop.width = maxX - minX;
        newDesktop.height = maxY - minY;

        // Check if desktop configuration changed
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mDesktopMutex);
            if (newDesktop.width != mCurrentDesktop.width ||
                newDesktop.height != mCurrentDesktop.height ||
                newDesktop.displays.size() != mCurrentDesktop.displays.size()) {
                mCurrentDesktop = newDesktop;
                changed = true;
            }
        }

        if (changed && onEvent) {
            mLogger.debug("Desktop changed: " + std::to_string(displayCount) + " displays, " +
                          std::to_string(newDesktop.width) + "x" + std::to_string(newDesktop.height));

            Event event {};
            event.type = EventType::DesktopChanged;
            event.timestamp = timestamp();
            event.state = getState();

            onEvent(event);
        }
    }

    void onDisplayConfigurationChanged() { updateDesktopInfo(); }

    static CGEventRef eventTapCallback(
        CGEventTapProxy /*proxy*/,
        CGEventType type,
        CGEventRef cgEvent,
        void *userInfo)
    {
        auto *platform = static_cast<MacOSPlatform *>(userInfo);

        if (!platform->onEvent) {
            return cgEvent;
        }

        Event event {};
        event.timestamp = timestamp();

        // Get current state
        event.state = platform->getState();

        // Update position from event
        CGPoint point = CGEventGetLocation(cgEvent);
        event.state.x = static_cast<int32_t>(point.x);
        event.state.y = static_cast<int32_t>(point.y);

        // Get delta from OS (works even when cursor is at screen edge)
        int64_t deltaX = CGEventGetIntegerValueField(cgEvent, kCGMouseEventDeltaX);
        int64_t deltaY = CGEventGetIntegerValueField(cgEvent, kCGMouseEventDeltaY);
        event.state.dx = static_cast<int32_t>(deltaX);
        event.state.dy = static_cast<int32_t>(deltaY);

        switch (type) {
            case kCGEventMouseMoved:
            case kCGEventLeftMouseDragged:
            case kCGEventRightMouseDragged:
            case kCGEventOtherMouseDragged:
                event.type = EventType::MouseMove;
                platform->onEvent(event);
                break;

            case kCGEventLeftMouseDown:
                event.type = EventType::MousePress;
                event.button = MouseButton::Left;
                platform->onEvent(event);
                break;

            case kCGEventLeftMouseUp:
                event.type = EventType::MouseRelease;
                event.button = MouseButton::Left;
                platform->onEvent(event);
                break;

            case kCGEventRightMouseDown:
                event.type = EventType::MousePress;
                event.button = MouseButton::Right;
                platform->onEvent(event);
                break;

            case kCGEventRightMouseUp:
                event.type = EventType::MouseRelease;
                event.button = MouseButton::Right;
                platform->onEvent(event);
                break;

            case kCGEventOtherMouseDown:
                event.type = EventType::MousePress;
                event.button = MouseButton::Middle;
                platform->onEvent(event);
                break;

            case kCGEventOtherMouseUp:
                event.type = EventType::MouseRelease;
                event.button = MouseButton::Middle;
                platform->onEvent(event);
                break;

            case kCGEventScrollWheel: {
                event.type = EventType::MouseScroll;
                // Get scroll deltas (continuous scrolling values)
                event.state.scrollX = CGEventGetDoubleValueField(cgEvent, kCGScrollWheelEventFixedPtDeltaAxis2);
                event.state.scrollY = CGEventGetDoubleValueField(cgEvent, kCGScrollWheelEventFixedPtDeltaAxis1);
                platform->onEvent(event);
                break;
            }

            case kCGEventKeyDown: {
                event.type = EventType::KeyPress;
                event.keycode = static_cast<uint32_t>(CGEventGetIntegerValueField(cgEvent, kCGKeyboardEventKeycode));

                // Try to get the text representation
                UniChar chars[4];
                UniCharCount length = 0;
                CGEventKeyboardGetUnicodeString(cgEvent, 4, &length, chars);
                if (length > 0) {
                    event.text = std::string(reinterpret_cast<char *>(chars), length * sizeof(UniChar));
                }

                platform->onEvent(event);
                break;
            }

            case kCGEventKeyUp: {
                event.type = EventType::KeyRelease;
                event.keycode = static_cast<uint32_t>(CGEventGetIntegerValueField(cgEvent, kCGKeyboardEventKeycode));

                // Try to get the text representation
                UniChar chars[4];
                UniCharCount length = 0;
                CGEventKeyboardGetUnicodeString(cgEvent, 4, &length, chars);
                if (length > 0) {
                    event.text = std::string(reinterpret_cast<char *>(chars), length * sizeof(UniChar));
                }

                platform->onEvent(event);
                break;
            }

            default:
                break;
        }

        return cgEvent;
    }

    void runEventLoop()
    {
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
            CGEventMaskBit(kCGEventScrollWheel) |
            CGEventMaskBit(kCGEventKeyDown) |
            CGEventMaskBit(kCGEventKeyUp);

        mEventTap = CGEventTapCreate(
            kCGSessionEventTap,
            kCGHeadInsertEventTap,
            kCGEventTapOptionListenOnly,
            event_mask,
            eventTapCallback,
            this);

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

    CFMachPortRef mEventTap { nullptr };
    CFRunLoopSourceRef mRunLoopSource { nullptr };
    CFRunLoopRef mEventLoop { nullptr };
    std::thread mListenerThread;
    std::atomic<bool> mIsRunning { false };
    Logger mLogger;
    bool mCursorVisible { true };

    // Desktop change monitoring
    mutable std::mutex mDesktopMutex;
    Desktop mCurrentDesktop;
};

std::unique_ptr<IPlatform> createPlatform()
{
    return std::make_unique<MacOSPlatform>();
}

} // namespace konflikt

#endif // __APPLE__
