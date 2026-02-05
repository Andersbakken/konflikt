// Linux platform implementation
// This is largely copied from the existing KonfliktNativeX11.cpp

#ifndef __APPLE__

#include "konflikt/Platform.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

// Workaround for xcb/xkb.h using 'explicit' as a struct field name
// which conflicts with the C++ keyword
#define explicit explicit_
#include <xcb/xkb.h>
#undef explicit

#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <xcb/xtest.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

namespace konflikt {

class LinuxPlatform : public IPlatform
{
public:
    LinuxPlatform() = default;

    ~LinuxPlatform() override { shutdown(); }

    bool initialize(const Logger &logger) override
    {
        mLogger = logger;

        // Connect to X server
        mConnection = xcb_connect(nullptr, &mDefaultScreen);
        if (xcb_connection_has_error(mConnection)) {
            mLogger.error("Failed to connect to X server");
            return false;
        }

        // Get screen info
        const xcb_setup_t *setup = xcb_get_setup(mConnection);
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
        for (int i = 0; i < mDefaultScreen; ++i) {
            xcb_screen_next(&iter);
        }
        mScreen = iter.data;

        // Initialize XKB
        if (!initXkb()) {
            mLogger.error("Failed to initialize XKB");
            return false;
        }

        // Initialize XInput2
        if (!initXInput()) {
            mLogger.error("Failed to initialize XInput2");
            return false;
        }

        // Create blank cursor for hiding
        createBlankCursor();

        // Update desktop info
        updateDesktopInfo();

        return true;
    }

    void shutdown() override
    {
        stopListening();

        if (mBlankCursor != XCB_NONE) {
            xcb_free_cursor(mConnection, mBlankCursor);
            mBlankCursor = XCB_NONE;
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

    InputState getState() const override
    {
        InputState state {};

        xcb_query_pointer_cookie_t cookie = xcb_query_pointer(mConnection, mScreen->root);
        xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(mConnection, cookie, nullptr);

        if (reply) {
            state.x = reply->root_x;
            state.y = reply->root_y;

            if (reply->mask & XCB_BUTTON_MASK_1)
                state.mouseButtons |= toUInt32(MouseButton::Left);
            if (reply->mask & XCB_BUTTON_MASK_2)
                state.mouseButtons |= toUInt32(MouseButton::Middle);
            if (reply->mask & XCB_BUTTON_MASK_3)
                state.mouseButtons |= toUInt32(MouseButton::Right);

            if (reply->mask & XCB_MOD_MASK_SHIFT)
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftShift);
            if (reply->mask & XCB_MOD_MASK_CONTROL)
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftControl);
            if (reply->mask & XCB_MOD_MASK_1)
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftAlt);
            if (reply->mask & XCB_MOD_MASK_4)
                state.keyboardModifiers |= toUInt32(KeyboardModifier::LeftSuper);
            if (reply->mask & XCB_MOD_MASK_LOCK)
                state.keyboardModifiers |= toUInt32(KeyboardModifier::CapsLock);
            if (reply->mask & XCB_MOD_MASK_2)
                state.keyboardModifiers |= toUInt32(KeyboardModifier::NumLock);

            free(reply);
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
        if (event.type == EventType::MouseMove) {
            xcb_warp_pointer(mConnection, XCB_NONE, mScreen->root, 0, 0, 0, 0, event.state.x, event.state.y);
        } else {
            uint8_t button = 1;
            if (event.button == MouseButton::Right)
                button = 3;
            else if (event.button == MouseButton::Middle)
                button = 2;

            bool isPress = event.type == EventType::MousePress;
            xcb_test_fake_input(mConnection, isPress ? XCB_BUTTON_PRESS : XCB_BUTTON_RELEASE, button, XCB_CURRENT_TIME, mScreen->root, event.state.x, event.state.y, 0);
        }
        xcb_flush(mConnection);
    }

    void sendKeyEvent(const Event &event) override
    {
        bool isPress = event.type == EventType::KeyPress;
        xcb_test_fake_input(mConnection, isPress ? XCB_KEY_PRESS : XCB_KEY_RELEASE, event.keycode + 8, XCB_CURRENT_TIME, mScreen->root, 0, 0, 0);
        xcb_flush(mConnection);
    }

    void startListening() override
    {
        if (mIsRunning)
            return;

        mIsRunning = true;
        mListenerThread = std::thread([this]() {
            eventLoop();
        });
    }

    void stopListening() override
    {
        if (!mIsRunning)
            return;

        mIsRunning = false;
        if (mListenerThread.joinable()) {
            mListenerThread.join();
        }
    }

    void showCursor() override
    {
        if (mCursorVisible)
            return;

        xcb_ungrab_pointer(mConnection, XCB_CURRENT_TIME);
        xcb_flush(mConnection);
        mCursorVisible = true;
    }

    void hideCursor() override
    {
        if (!mCursorVisible)
            return;

        if (mBlankCursor != XCB_NONE) {
            xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer(
                mConnection,
                1,
                mScreen->root,
                XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                XCB_GRAB_MODE_ASYNC,
                XCB_GRAB_MODE_ASYNC,
                XCB_NONE,
                mBlankCursor,
                XCB_CURRENT_TIME);

            xcb_grab_pointer_reply_t *reply = xcb_grab_pointer_reply(mConnection, cookie, nullptr);
            if (reply) {
                mCursorVisible = reply->status != XCB_GRAB_STATUS_SUCCESS;
                free(reply);
            }
            xcb_flush(mConnection);
        }
    }

    bool isCursorVisible() const override { return mCursorVisible; }

    std::string getClipboardText(ClipboardSelection /*selection*/) const override
    {
        // TODO: Implement X11 clipboard
        return "";
    }

    bool setClipboardText(const std::string & /*text*/, ClipboardSelection /*selection*/) override
    {
        // TODO: Implement X11 clipboard
        return false;
    }

private:
    bool initXkb()
    {
        mXkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (!mXkbContext)
            return false;

        // Enable XKB extension
        xcb_xkb_use_extension_cookie_t cookie =
            xcb_xkb_use_extension(mConnection, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        xcb_xkb_use_extension_reply_t *reply = xcb_xkb_use_extension_reply(mConnection, cookie, nullptr);

        if (!reply || !reply->supported) {
            free(reply);
            return false;
        }
        free(reply);

        return true;
    }

    bool initXInput()
    {
        // Query XInput extension
        xcb_input_xi_query_version_cookie_t cookie =
            xcb_input_xi_query_version(mConnection, XCB_INPUT_MAJOR_VERSION, XCB_INPUT_MINOR_VERSION);
        xcb_input_xi_query_version_reply_t *reply =
            xcb_input_xi_query_version_reply(mConnection, cookie, nullptr);

        if (!reply)
            return false;
        free(reply);

        // Select events on root window
        struct
        {
            xcb_input_event_mask_t header;
            uint32_t mask;
        } mask;

        mask.header.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
        mask.header.mask_len = 1;
        mask.mask = XCB_INPUT_XI_EVENT_MASK_RAW_MOTION | XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_PRESS |
            XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_RELEASE | XCB_INPUT_XI_EVENT_MASK_RAW_KEY_PRESS |
            XCB_INPUT_XI_EVENT_MASK_RAW_KEY_RELEASE;

        xcb_input_xi_select_events(mConnection, mScreen->root, 1, &mask.header);
        xcb_flush(mConnection);

        return true;
    }

    void createBlankCursor()
    {
        xcb_pixmap_t pixmap = xcb_generate_id(mConnection);
        mBlankCursor = xcb_generate_id(mConnection);

        xcb_create_pixmap(mConnection, 1, pixmap, mScreen->root, 1, 1);
        xcb_create_cursor(mConnection, mBlankCursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0);
        xcb_free_pixmap(mConnection, pixmap);
    }

    void updateDesktopInfo()
    {
        Desktop newDesktop;

        // Get screen dimensions using RANDR
        xcb_randr_get_screen_resources_cookie_t resCookie =
            xcb_randr_get_screen_resources(mConnection, mScreen->root);
        xcb_randr_get_screen_resources_reply_t *resources =
            xcb_randr_get_screen_resources_reply(mConnection, resCookie, nullptr);

        if (resources) {
            xcb_randr_crtc_t *crtcs = xcb_randr_get_screen_resources_crtcs(resources);
            int numCrtcs = xcb_randr_get_screen_resources_crtcs_length(resources);

            int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
            bool first = true;

            for (int i = 0; i < numCrtcs; i++) {
                xcb_randr_get_crtc_info_cookie_t crtcCookie =
                    xcb_randr_get_crtc_info(mConnection, crtcs[i], resources->config_timestamp);
                xcb_randr_get_crtc_info_reply_t *crtcInfo =
                    xcb_randr_get_crtc_info_reply(mConnection, crtcCookie, nullptr);

                if (crtcInfo && crtcInfo->width > 0 && crtcInfo->height > 0) {
                    Display display;
                    display.id = crtcs[i];
                    display.x = crtcInfo->x;
                    display.y = crtcInfo->y;
                    display.width = crtcInfo->width;
                    display.height = crtcInfo->height;
                    display.isPrimary = (i == 0);

                    newDesktop.displays.push_back(display);

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
                free(crtcInfo);
            }

            newDesktop.width = maxX - minX;
            newDesktop.height = maxY - minY;

            free(resources);
        } else {
            // Fallback to screen dimensions
            newDesktop.width = mScreen->width_in_pixels;
            newDesktop.height = mScreen->height_in_pixels;

            Display display;
            display.id = 0;
            display.x = 0;
            display.y = 0;
            display.width = newDesktop.width;
            display.height = newDesktop.height;
            display.isPrimary = true;
            newDesktop.displays.push_back(display);
        }

        std::lock_guard<std::mutex> lock(mDesktopMutex);
        mCurrentDesktop = newDesktop;
    }

    void eventLoop()
    {
        // Get XInput opcode
        const xcb_query_extension_reply_t *extReply = xcb_get_extension_data(mConnection, &xcb_input_id);
        uint8_t xiOpcode = extReply ? extReply->major_opcode : 0;

        while (mIsRunning) {
            xcb_generic_event_t *xcbEvent = xcb_poll_for_event(mConnection);
            if (!xcbEvent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint8_t responseType = xcbEvent->response_type & ~0x80;

            if (responseType == XCB_GE_GENERIC && xiOpcode) {
                auto *ge = reinterpret_cast<xcb_ge_generic_event_t *>(xcbEvent);
                if (ge->extension == xiOpcode) {
                    handleXInputEvent(ge);
                }
            }

            free(xcbEvent);
        }
    }

    void handleXInputEvent(xcb_ge_generic_event_t *ge)
    {
        Event event;
        event.timestamp = timestamp();

        switch (ge->event_type) {
            case XCB_INPUT_RAW_MOTION: {
                auto *raw = reinterpret_cast<xcb_input_raw_motion_event_t *>(ge);
                event.type = EventType::MouseMove;

            // Get axis values for delta
                xcb_input_fp3232_t *values = xcb_input_raw_button_press_axisvalues_raw(
                    reinterpret_cast<xcb_input_raw_button_press_event_t *>(raw));

                int numValues =
                    xcb_input_raw_button_press_axisvalues_raw_length(reinterpret_cast<xcb_input_raw_button_press_event_t *>(raw));

                if (numValues >= 2) {
                    event.state.dx = static_cast<int32_t>(values[0].integral);
                    event.state.dy = static_cast<int32_t>(values[1].integral);
                }

            // Get current position
                InputState state = getState();
                event.state.x = state.x;
                event.state.y = state.y;
                event.state.keyboardModifiers = state.keyboardModifiers;
                event.state.mouseButtons = state.mouseButtons;

                if (onEvent)
                    onEvent(event);
                break;
            }

            case XCB_INPUT_RAW_BUTTON_PRESS:
            case XCB_INPUT_RAW_BUTTON_RELEASE: {
                auto *raw = reinterpret_cast<xcb_input_raw_button_press_event_t *>(ge);
                event.type = ge->event_type == XCB_INPUT_RAW_BUTTON_PRESS ? EventType::MousePress : EventType::MouseRelease;

                if (raw->detail == 1)
                    event.button = MouseButton::Left;
                else if (raw->detail == 2)
                    event.button = MouseButton::Middle;
                else if (raw->detail == 3)
                    event.button = MouseButton::Right;
                else
                    break; // Ignore scroll wheel

                event.state = getState();
                if (onEvent)
                    onEvent(event);
                break;
            }

            case XCB_INPUT_RAW_KEY_PRESS:
            case XCB_INPUT_RAW_KEY_RELEASE: {
                auto *raw = reinterpret_cast<xcb_input_raw_key_press_event_t *>(ge);
                event.type = ge->event_type == XCB_INPUT_RAW_KEY_PRESS ? EventType::KeyPress : EventType::KeyRelease;
                event.keycode = raw->detail - 8; // Convert to Linux keycode

                event.state = getState();
                if (onEvent)
                    onEvent(event);
                break;
            }
        }
    }

    xcb_connection_t *mConnection { nullptr };
    xcb_screen_t *mScreen { nullptr };
    int mDefaultScreen { 0 };
    xcb_cursor_t mBlankCursor { XCB_NONE };

    xkb_context *mXkbContext { nullptr };

    Logger mLogger;
    mutable std::mutex mDesktopMutex;
    Desktop mCurrentDesktop;

    std::thread mListenerThread;
    std::atomic<bool> mIsRunning { false };
    bool mCursorVisible { true };
};

std::unique_ptr<IPlatform> createPlatform()
{
    return std::make_unique<LinuxPlatform>();
}

} // namespace konflikt

#endif // !__APPLE__
