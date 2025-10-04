#ifdef __linux__

#include "KonfliktNative.h"

#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <xcb/xtest.h>
// xcb/xkb.h uses 'explicit' as a variable name which conflicts with C++ keyword
// We only need xkbcommon, not the xcb xkb header
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

namespace konflikt {

class XCBHook : public IPlatformHook
{
public:
    XCBHook()                   = default;
    virtual ~XCBHook() override = default;

    virtual bool initialize(const Logger &logger) override
    {
        mLogger     = logger;
        // Connect to X server
        mConnection = xcb_connect(nullptr, &mScreenNumber);
        if (xcb_connection_has_error(mConnection)) {
            return false;
        }

        // Get screen
        const xcb_setup_t *setup   = xcb_get_setup(mConnection);
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
        for (int i = 0; i < mScreenNumber; i++) {
            xcb_screen_next(&iter);
        }
        mScreen = iter.data;

        if (!mScreen) {
            xcb_disconnect(mConnection);
            mConnection = nullptr;
            return false;
        }

        // Query XInput2 extension
        const xcb_query_extension_reply_t *xinput_ext = xcb_get_extension_data(mConnection, &xcb_input_id);
        if (!xinput_ext || !xinput_ext->present) {
            mLogger.error("XInput2 extension not available");
            xcb_disconnect(mConnection);
            mConnection = nullptr;
            return false;
        }
        mXinputOpcode     = xinput_ext->major_opcode;
        mXinputFirstEvent = xinput_ext->first_event;

        // Query XTest extension
        const xcb_query_extension_reply_t *xtest_ext = xcb_get_extension_data(mConnection, &xcb_test_id);
        if (!xtest_ext || !xtest_ext->present) {
            mLogger.error("XTest extension not available");
            xcb_disconnect(mConnection);
            mConnection = nullptr;
            return false;
        }

        // Setup XKB for key translation
        // First, initialize the XKB extension
        xkb_x11_setup_xkb_extension(mConnection,
                                    XKB_X11_MIN_MAJOR_XKB_VERSION,
                                    XKB_X11_MIN_MINOR_XKB_VERSION,
                                    XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);

        int32_t device_id = xkb_x11_get_core_keyboard_device_id(mConnection);
        if (device_id == -1) {
            mLogger.error("Failed to get keyboard device");
            xcb_disconnect(mConnection);
            mConnection = nullptr;
            return false;
        }

        mXkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (!mXkbContext) {
            mLogger.error("Failed to create XKB context");
            xcb_disconnect(mConnection);
            mConnection = nullptr;
            return false;
        }

        mXkbKeymap = xkb_x11_keymap_new_from_device(mXkbContext, mConnection, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (!mXkbKeymap) {
            mLogger.error("Failed to create XKB keymap");
            xkb_context_unref(mXkbContext);
            mXkbContext = nullptr;
            xcb_disconnect(mConnection);
            mConnection = nullptr;
            return false;
        }

        mXkbState = xkb_x11_state_new_from_device(mXkbKeymap, mConnection, device_id);
        if (!mXkbState) {
            mLogger.error("Failed to create XKB state");
            xkb_keymap_unref(mXkbKeymap);
            mXkbKeymap = nullptr;
            xkb_context_unref(mXkbContext);
            mXkbContext = nullptr;
            xcb_disconnect(mConnection);
            mConnection = nullptr;
            return false;
        }

        mIsRunning     = false;
        mCursorVisible = true;
        mBlankCursor   = XCB_NONE;
        return true;
    }

    virtual void shutdown() override
    {
        stopListening();

        if (mBlankCursor != XCB_NONE) {
            xcb_free_cursor(mConnection, mBlankCursor);
            mBlankCursor = XCB_NONE;
        }

        if (mXkbState) {
            xkb_state_unref(mXkbState);
            mXkbState = nullptr;
        }

        if (mXkbKeymap) {
            xkb_keymap_unref(mXkbKeymap);
            mXkbKeymap = nullptr;
        }

        if (mXkbContext) {
            xkb_context_unref(mXkbContext);
            mXkbContext = nullptr;
        }

        if (mConnection) {
            xcb_disconnect(mConnection);
            mConnection = nullptr;
        }
    }

    virtual State getState() const override
    {
        State state {};

        if (!mConnection || !mScreen) {
            return state;
        }

        // Query pointer
        xcb_query_pointer_cookie_t cookie = xcb_query_pointer(mConnection, mScreen->root);
        xcb_query_pointer_reply_t *reply  = xcb_query_pointer_reply(mConnection, cookie, nullptr);

        if (reply) {
            state.x = reply->root_x;
            state.y = reply->root_y;

            // Parse button mask
            if (reply->mask & XCB_BUTTON_MASK_1) {
                state.mouseButtons |= toUInt32(MouseButton::Left);
            }
            if (reply->mask & XCB_BUTTON_MASK_3) {
                state.mouseButtons |= toUInt32(MouseButton::Right);
            }
            if (reply->mask & XCB_BUTTON_MASK_2) {
                state.mouseButtons |= toUInt32(MouseButton::Middle);
            }

            // Parse modifier mask
            if (reply->mask & XCB_MOD_MASK_SHIFT) {
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftShift);
            }
            if (reply->mask & XCB_MOD_MASK_CONTROL) {
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftControl);
            }
            if (reply->mask & XCB_MOD_MASK_1) { // Alt
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftAlt);
            }
            if (reply->mask & XCB_MOD_MASK_4) { // Super
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftSuper);
            }
            if (reply->mask & XCB_MOD_MASK_LOCK) { // Caps Lock
                state.keyboardModifiers |= toUInt32(KeyboardModifier::CapsLock);
            }
            if (reply->mask & XCB_MOD_MASK_2) { // Num Lock
                state.keyboardModifiers |= toUInt32(KeyboardModifier::NumLock);
            }

            free(reply);
        }

        return state;
    }

    virtual Desktop getDesktop() const override
    {
        Desktop desktop {};

        if (!mScreen) {
            return desktop;
        }

        desktop.width  = mScreen->width_in_pixels;
        desktop.height = mScreen->height_in_pixels;

        return desktop;
    }

    virtual void sendMouseEvent(const Event &event) override
    {
        if (!mConnection) {
            return;
        }

        switch (event.type) {
            case EventType::MouseMove:
                xcb_test_fake_input(mConnection, XCB_MOTION_NOTIFY, 0, XCB_CURRENT_TIME, mScreen->root, event.state.x, event.state.y, 0);
                break;

            case EventType::MousePress: {
                uint8_t button = buttonFromMouseButton(event.button);
                xcb_test_fake_input(mConnection, XCB_BUTTON_PRESS, button, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
                break;
            }

            case EventType::MouseRelease: {
                uint8_t button = buttonFromMouseButton(event.button);
                xcb_test_fake_input(mConnection, XCB_BUTTON_RELEASE, button, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
                break;
            }

            default:
                break;
        }

        xcb_flush(mConnection);
    }

    virtual void sendKeyEvent(const Event &event) override
    {
        if (!mConnection) {
            return;
        }

        uint8_t event_type = (event.type == EventType::KeyPress) ? XCB_KEY_PRESS : XCB_KEY_RELEASE;
        xcb_test_fake_input(mConnection, event_type, event.keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
        xcb_flush(mConnection);
    }

    virtual void startListening() override
    {
        if (mIsRunning) {
            return;
        }

        mIsRunning = true;

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

        // Wait for thread to exit
        if (mListenerThread.joinable()) {
            mListenerThread.join();
        }
    }

    virtual void showCursor() override
    {
        if (!mConnection || !mScreen || mCursorVisible) {
            return;
        }

        xcb_change_window_attributes(mConnection, mScreen->root, XCB_CW_CURSOR, nullptr);
        xcb_flush(mConnection);
        mCursorVisible = true;
    }

    virtual void hideCursor() override
    {
        if (!mConnection || !mScreen || !mCursorVisible) {
            return;
        }

        if (mBlankCursor == XCB_NONE) {
            mBlankCursor = createBlankCursor();
        }

        if (mBlankCursor != XCB_NONE) {
            uint32_t values[] = { mBlankCursor };
            xcb_change_window_attributes(mConnection, mScreen->root, XCB_CW_CURSOR, values);
            xcb_flush(mConnection);
            mCursorVisible = false;
        }
    }

    virtual bool isCursorVisible() const override
    {
        return mCursorVisible;
    }

private:
    static uint8_t buttonFromMouseButton(MouseButton button)
    {
        switch (button) {
            case MouseButton::Left: return XCB_BUTTON_INDEX_1;
            case MouseButton::Right: return XCB_BUTTON_INDEX_3;
            case MouseButton::Middle: return XCB_BUTTON_INDEX_2;
            default: return XCB_BUTTON_INDEX_1;
        }
    }

    static MouseButton mouseButtonFromButton(uint32_t button)
    {
        switch (button) {
            case XCB_BUTTON_INDEX_1: return MouseButton::Left;
            case XCB_BUTTON_INDEX_3: return MouseButton::Right;
            case XCB_BUTTON_INDEX_2: return MouseButton::Middle;
            default: return static_cast<MouseButton>(0);
        }
    }

    void runEventLoop()
    {
        if (!mConnection) {
            mLogger.error("No connection in event loop");
            mIsRunning = false;
            return;
        }

        // Select XInput2 raw events
        // Use ALL_MASTER_DEVICES to avoid getting duplicate events from slave devices
        struct
        {
            xcb_input_event_mask_t header;
            uint32_t mask;
        } eventMask;

        eventMask.header.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
        eventMask.header.mask_len = 1;
        eventMask.mask            = XCB_INPUT_XI_EVENT_MASK_RAW_KEY_PRESS |
            XCB_INPUT_XI_EVENT_MASK_RAW_KEY_RELEASE |
            XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_PRESS |
            XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_RELEASE |
            XCB_INPUT_XI_EVENT_MASK_RAW_MOTION;

        xcb_void_cookie_t cookie = xcb_input_xi_select_events_checked(
            mConnection,
            mScreen->root,
            1,
            &eventMask.header);

        xcb_generic_error_t *error = xcb_request_check(mConnection, cookie);
        if (error) {
            mLogger.error("Failed to select XInput2 events: " + std::to_string(error->error_code));
            free(error);
            mIsRunning = false;
            return;
        }

        xcb_flush(mConnection);
        mLogger.debug("Starting XInput2 event loop...");

        // Event loop
        while (mIsRunning) {
            xcb_generic_event_t *event = xcb_poll_for_event(mConnection);

            if (!event) {
                // Check for connection errors
                int err = xcb_connection_has_error(mConnection);
                if (err) {
                    mLogger.error("XCB connection error: " + std::to_string(err));
                    break;
                }

                // No event, sleep briefly to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint8_t response_type = event->response_type & ~0x80;

            // Check if this is an XInput2 event
            if (response_type == XCB_GE_GENERIC) {
                xcb_ge_generic_event_t *ge = reinterpret_cast<xcb_ge_generic_event_t *>(event);

                if (ge->extension == mXinputOpcode) {
                    processXInputEvent(ge);
                }
            }

            free(event);
        }

        mLogger.debug("XInput2 event loop exited (is_running=" + std::to_string(mIsRunning.load()) + ")");
        mIsRunning = false;
    }

    void processXInputEvent(xcb_ge_generic_event_t *ge)
    {
        if (!eventCallback) {
            return;
        }

        Event event {};
        event.timestamp = timestamp();
        event.state     = getState();

        bool shouldDispatch = false;

        switch (ge->event_type) {
            case XCB_INPUT_RAW_KEY_PRESS: {
                auto *keyEvent = reinterpret_cast<xcb_input_raw_key_press_event_t *>(ge);
                event.type     = EventType::KeyPress;
                event.keycode  = keyEvent->detail;

                // Get text representation using xkbcommon
                if (mXkbState) {
                    xkb_keysym_t keysym = xkb_state_key_get_one_sym(mXkbState, event.keycode);
                    if (keysym != XKB_KEY_NoSymbol) {
                        char buf[32];
                        int len = xkb_keysym_to_utf8(keysym, buf, sizeof(buf));
                        if (len > 0 && len < (int)sizeof(buf)) {
                            event.text = std::string(buf, len);
                        }
                    }
                    // Update state for the key press
                    xkb_state_update_key(mXkbState, event.keycode, XKB_KEY_DOWN);
                }

                shouldDispatch = true;
                break;
            }

            case XCB_INPUT_RAW_KEY_RELEASE: {
                auto *keyEvent = reinterpret_cast<xcb_input_raw_key_release_event_t *>(ge);
                event.type     = EventType::KeyRelease;
                event.keycode  = keyEvent->detail;

                // Get text representation using xkbcommon
                if (mXkbState) {
                    xkb_keysym_t keysym = xkb_state_key_get_one_sym(mXkbState, event.keycode);
                    if (keysym != XKB_KEY_NoSymbol) {
                        char buf[32];
                        int len = xkb_keysym_to_utf8(keysym, buf, sizeof(buf));
                        if (len > 0 && len < (int)sizeof(buf)) {
                            event.text = std::string(buf, len);
                        }
                    }
                    // Update state for the key release
                    xkb_state_update_key(mXkbState, event.keycode, XKB_KEY_UP);
                }

                shouldDispatch = true;
                break;
            }

            case XCB_INPUT_RAW_BUTTON_PRESS: {
                auto *buttonEvent = reinterpret_cast<xcb_input_raw_button_press_event_t *>(ge);
                uint32_t button   = buttonEvent->detail;

                // Filter out scroll wheel events (buttons 4-7)
                if (button >= XCB_BUTTON_INDEX_1 && button <= XCB_BUTTON_INDEX_3) {
                    event.type     = EventType::MousePress;
                    event.button   = mouseButtonFromButton(button);
                    shouldDispatch = true;
                }
                break;
            }

            case XCB_INPUT_RAW_BUTTON_RELEASE: {
                auto *buttonEvent = reinterpret_cast<xcb_input_raw_button_release_event_t *>(ge);
                uint32_t button   = buttonEvent->detail;

                // Filter out scroll wheel events (buttons 4-7)
                if (button >= XCB_BUTTON_INDEX_1 && button <= XCB_BUTTON_INDEX_3) {
                    event.type     = EventType::MouseRelease;
                    event.button   = mouseButtonFromButton(button);
                    shouldDispatch = true;
                }
                break;
            }

            case XCB_INPUT_RAW_MOTION: {
                event.type     = EventType::MouseMove;
                shouldDispatch = true;
                break;
            }

            default:
                break;
        }

        if (shouldDispatch) {
            eventCallback(event);
        }
    }

    xcb_cursor_t createBlankCursor()
    {
        if (!mConnection || !mScreen) {
            return XCB_NONE;
        }

        xcb_pixmap_t pixmap = xcb_generate_id(mConnection);
        xcb_cursor_t cursor = xcb_generate_id(mConnection);

        xcb_create_pixmap(mConnection, 1, pixmap, mScreen->root, 1, 1);

        xcb_create_cursor(mConnection, cursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0);

        xcb_free_pixmap(mConnection, pixmap);
        return cursor;
    }

    xcb_connection_t *mConnection { nullptr };
    xcb_screen_t *mScreen { nullptr };
    int mScreenNumber { 0 };
    uint8_t mXinputOpcode { 0 };
    uint8_t mXinputFirstEvent { 0 };
    Logger mLogger;

    struct xkb_context *mXkbContext { nullptr };
    struct xkb_keymap *mXkbKeymap { nullptr };
    struct xkb_state *mXkbState { nullptr };

    std::thread mListenerThread;
    std::atomic<bool> mIsRunning { false };

    bool mCursorVisible { true };
    xcb_cursor_t mBlankCursor { XCB_NONE };
};

std::unique_ptr<IPlatformHook> createPlatformHook()
{
    return std::make_unique<XCBHook>();
}

} // namespace konflikt

#endif // __linux__
