#ifdef __APPLE__

#include "KonfliktNative.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace konflikt {

class MacOSHook : public IPlatformHook
{
public:
    MacOSHook()           = default;
    ~MacOSHook() override = default;

    virtual bool initialize(const Logger &logger) override
    {
        mLogger        = logger;
        mEventTap      = nullptr;
        mRunLoopSource = nullptr;
        mEventLoop     = nullptr;
        mIsRunning     = false;
        mCursorVisible = true;
        
        // Initialize current desktop state
        CGDirectDisplayID display = CGMainDisplayID();
        mCurrentDesktop.width = static_cast<int32_t>(CGDisplayPixelsWide(display));
        mCurrentDesktop.height = static_cast<int32_t>(CGDisplayPixelsHigh(display));
        
        // Register for display configuration changes
        CGDisplayRegisterReconfigurationCallback(displayConfigurationCallback, this);
        
        return true;
    }

    virtual void shutdown() override
    {
        stopListening();
        
        // Unregister display configuration callback
        CGDisplayRemoveReconfigurationCallback(displayConfigurationCallback, this);
    }

    virtual State getState() const override
    {
        State state {};

        // Get mouse position
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint point    = CGEventGetLocation(event);
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
            // Note: Can't distinguish left/right shift easily
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

    virtual Desktop getDesktop() const override
    {
        std::lock_guard<std::mutex> lock(mDesktopMutex);
        return mCurrentDesktop;
    }

    virtual void sendMouseEvent(const Event &event) override
    {
        CGPoint pos             = CGPointMake(event.state.x, event.state.y);
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

        CGEventRef cgEvent = nullptr;

        switch (event.type) {
            case EventType::MouseMove: {
                cgEvent = CGEventCreateMouseEvent(source, kCGEventMouseMoved, pos, kCGMouseButtonLeft);
                break;
            }
            case EventType::MousePress: {
                CGEventType type     = kCGEventLeftMouseDown;
                CGMouseButton button = kCGMouseButtonLeft;

                if (event.button == MouseButton::Right) {
                    type   = kCGEventRightMouseDown;
                    button = kCGMouseButtonRight;
                } else if (event.button == MouseButton::Middle) {
                    type   = kCGEventOtherMouseDown;
                    button = kCGMouseButtonCenter;
                }

                cgEvent = CGEventCreateMouseEvent(source, type, pos, button);
                break;
            }
            case EventType::MouseRelease: {
                CGEventType type     = kCGEventLeftMouseUp;
                CGMouseButton button = kCGMouseButtonLeft;

                if (event.button == MouseButton::Right) {
                    type   = kCGEventRightMouseUp;
                    button = kCGMouseButtonRight;
                } else if (event.button == MouseButton::Middle) {
                    type   = kCGEventOtherMouseUp;
                    button = kCGMouseButtonCenter;
                }

                cgEvent = CGEventCreateMouseEvent(source, type, pos, button);
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

    virtual void sendKeyEvent(const Event &event) override
    {
        CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

        bool isDown        = (event.type == EventType::KeyPress);
        CGEventRef cgEvent = CGEventCreateKeyboardEvent(source, event.keycode, isDown);

        if (cgEvent) {
            CGEventPost(kCGHIDEventTap, cgEvent);
            CFRelease(cgEvent);
        }

        CFRelease(source);
    }

    virtual void startListening() override
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

    virtual void stopListening() override
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

    virtual void showCursor() override
    {
        if (mCursorVisible) {
            return;
        }

        CGDisplayShowCursor(kCGDirectMainDisplay);
        mCursorVisible = true;
    }

    virtual void hideCursor() override
    {
        if (!mCursorVisible) {
            return;
        }

        CGDisplayHideCursor(kCGDirectMainDisplay);
        mCursorVisible = false;
    }

    virtual bool isCursorVisible() const override
    {
        return mCursorVisible;
    }

    virtual std::string getClipboardText(ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        // macOS only has system clipboard, ignore Primary selection
        (void)selection; // Suppress unused parameter warning
        
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSString *string = [pasteboard stringForType:NSPasteboardTypeString];
        
        if (string) {
            return std::string([string UTF8String]);
        }
        
        return "";
    }

    virtual bool setClipboardText(const std::string &text, ClipboardSelection selection = ClipboardSelection::Auto) override
    {
        // macOS only has system clipboard, ignore Primary selection
        (void)selection; // Suppress unused parameter warning
        
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        
        NSString *nsString = [NSString stringWithUTF8String:text.c_str()];
        BOOL success = [pasteboard setString:nsString forType:NSPasteboardTypeString];
        
        return success == YES;
    }

    virtual std::vector<uint8_t> getClipboardData(const std::string &mimeType, ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        (void)selection; // macOS only has system clipboard
        
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        
        // Convert MIME type to macOS pasteboard type
        std::string macType = MimeTypeMapper::mimeToMacType(mimeType);
        NSString *nsType = [NSString stringWithUTF8String:macType.c_str()];
        
        // Special handling for text types
        if (mimeType == "text/plain" || mimeType == "text/plain;charset=utf-8") {
            std::string text = getClipboardText(selection);
            return std::vector<uint8_t>(text.begin(), text.end());
        }
        
        // Special handling for JPEG conversion from TIFF
        if (mimeType == "image/jpeg" || mimeType == "image/jpg") {
            NSData *data = [pasteboard dataForType:NSPasteboardTypeTIFF];
            if (data) {
                NSImage *image = [[NSImage alloc] initWithData:data];
                if (image) {
                    NSBitmapImageRep *bitmap = [NSBitmapImageRep imageRepWithData:[image TIFFRepresentation]];
                    if (bitmap) {
                        NSData *jpegData = [bitmap representationUsingType:NSBitmapImageFileTypeJPEG properties:@{}];
                        if (jpegData) {
                            const uint8_t *bytes = static_cast<const uint8_t *>([jpegData bytes]);
                            return std::vector<uint8_t>(bytes, bytes + [jpegData length]);
                        }
                    }
                }
            }
            return {};
        }
        
        // Generic data retrieval for all other types
        NSData *data = [pasteboard dataForType:nsType];
        if (data) {
            const uint8_t *bytes = static_cast<const uint8_t *>([data bytes]);
            return std::vector<uint8_t>(bytes, bytes + [data length]);
        }
        
        return {};
    }

    virtual bool setClipboardData(const std::string &mimeType, const std::vector<uint8_t> &data, ClipboardSelection selection = ClipboardSelection::Auto) override
    {
        (void)selection; // macOS only has system clipboard
        
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        
        // Special handling for text types
        if (mimeType == "text/plain" || mimeType == "text/plain;charset=utf-8") {
            std::string text(data.begin(), data.end());
            return setClipboardText(text, selection);
        }
        
        // Special handling for JPEG conversion to TIFF
        if (mimeType == "image/jpeg" || mimeType == "image/jpg") {
            NSData *jpegData = [NSData dataWithBytes:data.data() length:data.size()];
            NSImage *image = [[NSImage alloc] initWithData:jpegData];
            if (image) {
                NSData *tiffData = [image TIFFRepresentation];
                if (tiffData) {
                    return [pasteboard setData:tiffData forType:NSPasteboardTypeTIFF];
                }
            }
            return false;
        }
        
        // Generic data setting for all other types
        std::string macType = MimeTypeMapper::mimeToMacType(mimeType);
        NSString *nsType = [NSString stringWithUTF8String:macType.c_str()];
        NSData *nsData = [NSData dataWithBytes:data.data() length:data.size()];
        
        return [pasteboard setData:nsData forType:nsType];
    }

    virtual std::vector<std::string> getClipboardMimeTypes(ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        (void)selection; // macOS only has system clipboard
        
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSArray *types = [pasteboard types];
        std::vector<std::string> mimeTypes;
        
        for (NSString *type in types) {
            std::string macType = [type UTF8String];
            std::string mimeType = MimeTypeMapper::macTypeToMime(macType);
            
            // Add the converted MIME type
            if (!mimeType.empty() && std::find(mimeTypes.begin(), mimeTypes.end(), mimeType) == mimeTypes.end()) {
                mimeTypes.push_back(mimeType);
            }
            
            // Special case: TIFF can be converted to JPEG
            if ([type isEqualToString:NSPasteboardTypeTIFF]) {
                if (std::find(mimeTypes.begin(), mimeTypes.end(), "image/jpeg") == mimeTypes.end()) {
                    mimeTypes.push_back("image/jpeg");
                }
            }
            
            // Special case: text types
            if ([type isEqualToString:NSPasteboardTypeString]) {
                if (std::find(mimeTypes.begin(), mimeTypes.end(), "text/plain;charset=utf-8") == mimeTypes.end()) {
                    mimeTypes.push_back("text/plain;charset=utf-8");
                }
            }
        }
        
        return mimeTypes;
    }

private:
    static void displayConfigurationCallback(
        CGDirectDisplayID /*display*/,
        CGDisplayChangeSummaryFlags flags,
        void *userInfo)
    {
        auto *hook = static_cast<MacOSHook *>(userInfo);
        
        // Only respond to configuration changes (not just enable/disable)
        if (flags & kCGDisplayDesktopShapeChangedFlag) {
            hook->onDisplayConfigurationChanged();
        }
    }
    
    void onDisplayConfigurationChanged()
    {
        Desktop newDesktop;
        CGDirectDisplayID display = CGMainDisplayID();
        newDesktop.width = static_cast<int32_t>(CGDisplayPixelsWide(display));
        newDesktop.height = static_cast<int32_t>(CGDisplayPixelsHigh(display));
        
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mDesktopMutex);
            if (newDesktop.width != mCurrentDesktop.width || 
                newDesktop.height != mCurrentDesktop.height) {
                mCurrentDesktop = newDesktop;
                changed = true;
            }
        }
        
        if (changed && eventCallback) {
            mLogger.debug("Desktop changed to " + std::to_string(newDesktop.width) + 
                         "x" + std::to_string(newDesktop.height));
                         
            Event event {};
            event.type = EventType::DesktopChanged;
            event.timestamp = timestamp();
            event.state = getState();
            
            eventCallback(event);
        }
    }

    static CGEventRef eventTapCallback(
        CGEventTapProxy /*proxy*/,
        CGEventType type,
        CGEventRef cgEvent,
        void *userInfo)
    {
        auto *hook = static_cast<MacOSHook *>(userInfo);

        if (!hook->eventCallback) {
            return cgEvent;
        }

        Event event {};
        event.timestamp = timestamp();

        // Get current state
        event.state = hook->getState();

        // Update position from event
        CGPoint point = CGEventGetLocation(cgEvent);
        event.state.x = static_cast<int32_t>(point.x);
        event.state.y = static_cast<int32_t>(point.y);

        switch (type) {
            case kCGEventMouseMoved:
            case kCGEventLeftMouseDragged:
            case kCGEventRightMouseDragged:
            case kCGEventOtherMouseDragged:
                event.type = EventType::MouseMove;
                hook->eventCallback(event);
                break;

            case kCGEventLeftMouseDown:
                event.type   = EventType::MousePress;
                event.button = MouseButton::Left;
                hook->eventCallback(event);
                break;

            case kCGEventLeftMouseUp:
                event.type   = EventType::MouseRelease;
                event.button = MouseButton::Left;
                hook->eventCallback(event);
                break;

            case kCGEventRightMouseDown:
                event.type   = EventType::MousePress;
                event.button = MouseButton::Right;
                hook->eventCallback(event);
                break;

            case kCGEventRightMouseUp:
                event.type   = EventType::MouseRelease;
                event.button = MouseButton::Right;
                hook->eventCallback(event);
                break;

            case kCGEventOtherMouseDown:
                event.type   = EventType::MousePress;
                event.button = MouseButton::Middle;
                hook->eventCallback(event);
                break;

            case kCGEventOtherMouseUp:
                event.type   = EventType::MouseRelease;
                event.button = MouseButton::Middle;
                hook->eventCallback(event);
                break;

            case kCGEventKeyDown: {
                event.type    = EventType::KeyPress;
                event.keycode = static_cast<uint32_t>(CGEventGetIntegerValueField(cgEvent, kCGKeyboardEventKeycode));

                // Try to get the text representation
                UniChar chars[4];
                UniCharCount length = 0;
                CGEventKeyboardGetUnicodeString(cgEvent, 4, &length, chars);
                if (length > 0) {
                    event.text = std::string(reinterpret_cast<char *>(chars), length * sizeof(UniChar));
                }

                hook->eventCallback(event);
                break;
            }

            case kCGEventKeyUp: {
                event.type    = EventType::KeyRelease;
                event.keycode = static_cast<uint32_t>(CGEventGetIntegerValueField(cgEvent, kCGKeyboardEventKeycode));

                // Try to get the text representation
                UniChar chars[4];
                UniCharCount length = 0;
                CGEventKeyboardGetUnicodeString(cgEvent, 4, &length, chars);
                if (length > 0) {
                    event.text = std::string(reinterpret_cast<char *>(chars), length * sizeof(UniChar));
                }

                hook->eventCallback(event);
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
        mEventLoop     = CFRunLoopGetCurrent();
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
    bool mCursorVisible { true };
    
    // Desktop change monitoring
    mutable std::mutex mDesktopMutex;
    Desktop mCurrentDesktop;
};

std::unique_ptr<IPlatformHook> createPlatformHook()
{
    return std::make_unique<MacOSHook>();
}

} // namespace konflikt

#endif // __APPLE__
