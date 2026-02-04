#ifdef __linux__

#include "KonfliktNative.h"

#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <xcb/xtest.h>
// xcb/xkb.h uses 'explicit' as a variable name which conflicts with C++ keyword
// We only need xkbcommon, not the xcb xkb header
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
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

        mIsRunning         = false;
        mListeningForInput = false;
        mCursorVisible     = true;
        mBlankCursor       = XCB_NONE;
        mClipboardWindow   = XCB_NONE;

        // Initialize current desktop state with all displays
        updateDesktopInfo();

        // Register for RandR events to monitor display configuration changes
        xcb_randr_select_input(mConnection, mScreen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE | XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE | XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE);

        // Also register for structure events on root window
        uint32_t values[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY };
        xcb_change_window_attributes(mConnection, mScreen->root, XCB_CW_EVENT_MASK, values);
        xcb_flush(mConnection);

        // Start the event loop immediately - it will always run for clipboard support
        startEventLoop();

        return true;
    }

    virtual void shutdown() override
    {
        stopEventLoop();

        // Ungrab pointer if we have it grabbed
        if (!mCursorVisible && mConnection) {
            xcb_ungrab_pointer(mConnection, XCB_CURRENT_TIME);
            xcb_flush(mConnection);
        }

        if (mBlankCursor != XCB_NONE) {
            xcb_free_cursor(mConnection, mBlankCursor);
            mBlankCursor = XCB_NONE;
        }

        if (mClipboardWindow != XCB_NONE) {
            xcb_destroy_window(mConnection, mClipboardWindow);
            mClipboardWindow = XCB_NONE;
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
        std::lock_guard<std::mutex> lock(mDesktopMutex);
        return mCurrentDesktop;
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
        // Enable input event processing in the already-running event loop
        mListeningForInput = true;

        // If event loop isn't running yet, start it
        if (!mIsRunning) {
            startEventLoop();
            mListeningForInput = true;
        }
    }

    virtual void stopListening() override
    {
        // Don't stop the event loop completely - just disable input listening
        mListeningForInput = false;
    }

    virtual void showCursor() override
    {
        if (!mConnection || !mScreen || mCursorVisible) {
            return;
        }

        // Ungrab the pointer to show the cursor again
        xcb_ungrab_pointer(mConnection, XCB_CURRENT_TIME);
        xcb_flush(mConnection);
        mCursorVisible = true;
        mLogger.debug("Cursor shown (pointer ungrabbed)");
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
            // Grab the pointer with a blank cursor to hide it system-wide
            // owner_events=1 means events are reported normally (so XInput2 raw events work)
            // but the cursor image is still the grabbed cursor (blank)
            xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer(
                mConnection,
                1,  // owner_events: true - events reported normally, but cursor is blank
                mScreen->root,
                XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                XCB_GRAB_MODE_ASYNC,  // pointer_mode
                XCB_GRAB_MODE_ASYNC,  // keyboard_mode
                XCB_NONE,             // confine_to: don't confine
                mBlankCursor,         // cursor: blank cursor
                XCB_CURRENT_TIME
            );

            xcb_grab_pointer_reply_t *reply = xcb_grab_pointer_reply(mConnection, cookie, nullptr);
            if (reply) {
                if (reply->status == XCB_GRAB_STATUS_SUCCESS) {
                    mCursorVisible = false;
                    mLogger.debug("Cursor hidden (pointer grabbed)");
                } else {
                    mLogger.error("Failed to grab pointer, status: " + std::to_string(reply->status));
                }
                free(reply);
            } else {
                mLogger.error("Failed to get grab pointer reply");
            }
            xcb_flush(mConnection);
        }
    }

    virtual bool isCursorVisible() const override
    {
        return mCursorVisible;
    }

    virtual std::string getClipboardText(ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        switch (selection) {
            case ClipboardSelection::Clipboard:
                return getSelectionText("CLIPBOARD");
            case ClipboardSelection::Primary:
                return getSelectionText("PRIMARY");
            case ClipboardSelection::Auto:
            default:
                // Auto: try CLIPBOARD first, fallback to PRIMARY
                std::string result = getSelectionText("CLIPBOARD");
                if (result.empty()) {
                    result = getSelectionText("PRIMARY");
                }
                return result;
        }
    }

    virtual bool setClipboardText(const std::string &text, ClipboardSelection selection = ClipboardSelection::Auto) override
    {
        if (!mConnection || !mScreen) {
            return false;
        }

        xcb_atom_t clipboard_atom = getAtom("CLIPBOARD");
        xcb_atom_t primary_atom   = getAtom("PRIMARY");

        // Store the text for future selection requests
        mClipboardText = text;

        // Create a window to own the selection
        if (mClipboardWindow == XCB_NONE) {
            mClipboardWindow  = xcb_generate_id(mConnection);
            uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
            xcb_create_window(mConnection, XCB_COPY_FROM_PARENT, mClipboardWindow, mScreen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, mScreen->root_visual, XCB_CW_EVENT_MASK, values);
        }

        bool success = false;

        switch (selection) {
            case ClipboardSelection::Clipboard:
                if (clipboard_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, clipboard_atom, XCB_CURRENT_TIME);
                    success = verifySelectionOwnership(clipboard_atom);
                }
                break;
            case ClipboardSelection::Primary:
                if (primary_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, primary_atom, XCB_CURRENT_TIME);
                    success = verifySelectionOwnership(primary_atom);
                }
                break;
            case ClipboardSelection::Auto:
            default:
                // Auto: set both CLIPBOARD and PRIMARY selections
                if (clipboard_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, clipboard_atom, XCB_CURRENT_TIME);
                }
                if (primary_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, primary_atom, XCB_CURRENT_TIME);
                }
                // Consider success if we own at least the clipboard
                success = clipboard_atom != XCB_NONE ? verifySelectionOwnership(clipboard_atom) : verifySelectionOwnership(primary_atom);
                break;
        }

        xcb_flush(mConnection);
        return success;
    }

    virtual std::vector<uint8_t> getClipboardData(const std::string &mimeType, ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        if (mimeType == "text/plain" || mimeType == "text/plain;charset=utf-8") {
            std::string text = getClipboardText(selection);
            return std::vector<uint8_t>(text.begin(), text.end());
        }

        // Try to get data for specific MIME type
        const char *selectionName = (selection == ClipboardSelection::Primary) ? "PRIMARY" : "CLIPBOARD";
        if (selection == ClipboardSelection::Auto) {
            // Try CLIPBOARD first, fallback to PRIMARY
            std::vector<uint8_t> result = getSelectionData("CLIPBOARD", mimeType);
            if (result.empty()) {
                result = getSelectionData("PRIMARY", mimeType);
            }
            return result;
        } else {
            return getSelectionData(selectionName, mimeType);
        }
    }

    virtual bool setClipboardData(const std::string &mimeType, const std::vector<uint8_t> &data, ClipboardSelection selection = ClipboardSelection::Auto) override
    {
        if (mimeType == "text/plain" || mimeType == "text/plain;charset=utf-8") {
            std::string text(data.begin(), data.end());
            return setClipboardText(text, selection);
        }

        if (!mConnection || !mScreen) {
            return false;
        }

        xcb_atom_t clipboard_atom = getAtom("CLIPBOARD");
        xcb_atom_t primary_atom   = getAtom("PRIMARY");

        // Store the data for future selection requests
        mClipboardData[mimeType] = data;

        // Also store as text if it's a text type for backwards compatibility
        if (mimeType.find("text/") == 0) {
            mClipboardText = std::string(data.begin(), data.end());
        }

        // Create a window to own the selection
        if (mClipboardWindow == XCB_NONE) {
            mClipboardWindow  = xcb_generate_id(mConnection);
            uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
            xcb_create_window(mConnection, XCB_COPY_FROM_PARENT, mClipboardWindow, mScreen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, mScreen->root_visual, XCB_CW_EVENT_MASK, values);
        }

        bool success = false;

        switch (selection) {
            case ClipboardSelection::Clipboard:
                if (clipboard_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, clipboard_atom, XCB_CURRENT_TIME);
                    success = verifySelectionOwnership(clipboard_atom);
                }
                break;
            case ClipboardSelection::Primary:
                if (primary_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, primary_atom, XCB_CURRENT_TIME);
                    success = verifySelectionOwnership(primary_atom);
                }
                break;
            case ClipboardSelection::Auto:
            default:
                // Auto: set both CLIPBOARD and PRIMARY selections
                if (clipboard_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, clipboard_atom, XCB_CURRENT_TIME);
                }
                if (primary_atom != XCB_NONE) {
                    xcb_set_selection_owner(mConnection, mClipboardWindow, primary_atom, XCB_CURRENT_TIME);
                }
                success = clipboard_atom != XCB_NONE ? verifySelectionOwnership(clipboard_atom) : verifySelectionOwnership(primary_atom);
                break;
        }

        xcb_flush(mConnection);
        return success;
    }

    virtual std::vector<std::string> getClipboardMimeTypes(ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        const char *selectionName = (selection == ClipboardSelection::Primary) ? "PRIMARY" : "CLIPBOARD";

        if (selection == ClipboardSelection::Auto) {
            // Try CLIPBOARD first, fallback to PRIMARY
            std::vector<std::string> types = getSelectionMimeTypes("CLIPBOARD");
            if (types.empty()) {
                types = getSelectionMimeTypes("PRIMARY");
            }
            return types;
        } else {
            return getSelectionMimeTypes(selectionName);
        }
    }

private:
    std::string getSelectionText(const char *selection_name) const
    {
        xcb_atom_t selection_atom  = getAtom(selection_name);
        xcb_atom_t utf8_atom       = getAtom("UTF8_STRING");
        xcb_atom_t text_atom       = getAtom("TEXT");
        xcb_atom_t target_property = getAtom("KONFLIKT_SELECTION_DATA");

        if (selection_atom == XCB_NONE || target_property == XCB_NONE) {
            return "";
        }

        // Create a temporary window to receive the selection data
        xcb_window_t window = xcb_generate_id(mConnection);
        uint32_t values[]   = { XCB_EVENT_MASK_PROPERTY_CHANGE };
        xcb_create_window(mConnection, XCB_COPY_FROM_PARENT, window, mScreen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, mScreen->root_visual, XCB_CW_EVENT_MASK, values);

        // Try UTF8_STRING first, then TEXT, then STRING
        xcb_atom_t targets[] = { utf8_atom, text_atom, XCB_ATOM_STRING };
        std::string result;

        for (xcb_atom_t target : targets) {
            if (target == XCB_NONE)
                continue;

            // Request selection content
            xcb_convert_selection(mConnection, window, selection_atom, target, target_property, XCB_CURRENT_TIME);
            xcb_flush(mConnection);

            bool received      = false;
            auto start_time    = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(500);

            // Wait for SelectionNotify event
            while (!received && (std::chrono::steady_clock::now() - start_time) < timeout) {
                xcb_generic_event_t *event = xcb_poll_for_event(mConnection);
                if (!event) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                uint8_t response_type = event->response_type & ~0x80;
                if (response_type == XCB_SELECTION_NOTIFY) {
                    xcb_selection_notify_event_t *se = (xcb_selection_notify_event_t *)event;
                    if (se->selection == selection_atom && se->property != XCB_NONE) {
                        // Get the actual data
                        xcb_get_property_cookie_t cookie = xcb_get_property(mConnection, 1, window, target_property, XCB_ATOM_ANY, 0, UINT32_MAX);
                        xcb_get_property_reply_t *reply  = xcb_get_property_reply(mConnection, cookie, nullptr);

                        if (reply && xcb_get_property_value_length(reply) > 0) {
                            const char *data = (const char *)xcb_get_property_value(reply);
                            int length       = xcb_get_property_value_length(reply);
                            result           = std::string(data, length);
                        }

                        if (reply) {
                            free(reply);
                        }
                    }
                    received = true;
                }
                free(event);
            }

            if (!result.empty()) {
                break;
            }
        }

        // Clean up window
        xcb_destroy_window(mConnection, window);
        xcb_flush(mConnection);

        return result;
    }

    xcb_atom_t getAtom(const char *name) const
    {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(mConnection, 0, strlen(name), name);
        xcb_intern_atom_reply_t *reply  = xcb_intern_atom_reply(mConnection, cookie, nullptr);

        xcb_atom_t atom = XCB_NONE;
        if (reply) {
            atom = reply->atom;
            free(reply);
        }
        return atom;
    }

    bool verifySelectionOwnership(xcb_atom_t selection_atom) const
    {
        xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(mConnection, selection_atom);
        xcb_get_selection_owner_reply_t *reply  = xcb_get_selection_owner_reply(mConnection, cookie, nullptr);

        bool success = false;
        if (reply) {
            success = (reply->owner == mClipboardWindow);
            free(reply);
        }
        return success;
    }

    std::vector<uint8_t> getSelectionData(const char *selection_name, const std::string &mimeType) const
    {
        xcb_atom_t selection_atom = getAtom(selection_name);

        // Convert MIME type to X11 selection target atom
        std::string x11Type        = MimeTypeMapper::mimeToX11Type(mimeType);
        xcb_atom_t target_atom     = getAtom(x11Type.c_str());
        xcb_atom_t target_property = getAtom("KONFLIKT_BINARY_DATA");

        if (selection_atom == XCB_NONE || target_atom == XCB_NONE || target_property == XCB_NONE) {
            return {};
        }

        // Create a temporary window to receive the selection data
        xcb_window_t window = xcb_generate_id(mConnection);
        uint32_t values[]   = { XCB_EVENT_MASK_PROPERTY_CHANGE };
        xcb_create_window(mConnection, XCB_COPY_FROM_PARENT, window, mScreen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, mScreen->root_visual, XCB_CW_EVENT_MASK, values);

        // Request selection content
        xcb_convert_selection(mConnection, window, selection_atom, target_atom, target_property, XCB_CURRENT_TIME);
        xcb_flush(mConnection);

        std::vector<uint8_t> result;
        bool received      = false;
        auto start_time    = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(500);

        // Wait for SelectionNotify event
        while (!received && (std::chrono::steady_clock::now() - start_time) < timeout) {
            xcb_generic_event_t *event = xcb_poll_for_event(mConnection);
            if (!event) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint8_t response_type = event->response_type & ~0x80;
            if (response_type == XCB_SELECTION_NOTIFY) {
                xcb_selection_notify_event_t *se = (xcb_selection_notify_event_t *)event;
                if (se->selection == selection_atom && se->property != XCB_NONE) {
                    // Get the actual data
                    xcb_get_property_cookie_t cookie = xcb_get_property(mConnection, 1, window, target_property, XCB_ATOM_ANY, 0, UINT32_MAX);
                    xcb_get_property_reply_t *reply  = xcb_get_property_reply(mConnection, cookie, nullptr);

                    if (reply && xcb_get_property_value_length(reply) > 0) {
                        const uint8_t *data = (const uint8_t *)xcb_get_property_value(reply);
                        int length          = xcb_get_property_value_length(reply);
                        result              = std::vector<uint8_t>(data, data + length);
                    }

                    if (reply) {
                        free(reply);
                    }
                }
                received = true;
            }
            free(event);
        }

        // Clean up window
        xcb_destroy_window(mConnection, window);
        xcb_flush(mConnection);

        return result;
    }

    std::vector<std::string> getSelectionMimeTypes(const char *selection_name) const
    {
        xcb_atom_t selection_atom  = getAtom(selection_name);
        xcb_atom_t targets_atom    = getAtom("TARGETS");
        xcb_atom_t target_property = getAtom("KONFLIKT_TARGETS");

        if (selection_atom == XCB_NONE || targets_atom == XCB_NONE || target_property == XCB_NONE) {
            return {};
        }

        // Create a temporary window to receive the targets
        xcb_window_t window = xcb_generate_id(mConnection);
        uint32_t values[]   = { XCB_EVENT_MASK_PROPERTY_CHANGE };
        xcb_create_window(mConnection, XCB_COPY_FROM_PARENT, window, mScreen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, mScreen->root_visual, XCB_CW_EVENT_MASK, values);

        // Request available targets
        xcb_convert_selection(mConnection, window, selection_atom, targets_atom, target_property, XCB_CURRENT_TIME);
        xcb_flush(mConnection);

        std::vector<std::string> mimeTypes;
        bool received      = false;
        auto start_time    = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(500);

        // Wait for SelectionNotify event
        while (!received && (std::chrono::steady_clock::now() - start_time) < timeout) {
            xcb_generic_event_t *event = xcb_poll_for_event(mConnection);
            if (!event) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint8_t response_type = event->response_type & ~0x80;
            if (response_type == XCB_SELECTION_NOTIFY) {
                xcb_selection_notify_event_t *se = (xcb_selection_notify_event_t *)event;
                if (se->selection == selection_atom && se->property != XCB_NONE) {
                    // Get the targets list
                    xcb_get_property_cookie_t cookie = xcb_get_property(mConnection, 1, window, target_property, XCB_ATOM_ATOM, 0, UINT32_MAX);
                    xcb_get_property_reply_t *reply  = xcb_get_property_reply(mConnection, cookie, nullptr);

                    if (reply && xcb_get_property_value_length(reply) > 0) {
                        xcb_atom_t *atoms = (xcb_atom_t *)xcb_get_property_value(reply);
                        int count         = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

                        for (int i = 0; i < count; ++i) {
                            std::string atomName = getAtomName(atoms[i]);
                            if (!atomName.empty()) {
                                // Convert X11 atom names to MIME types using mapper
                                std::string mimeType = MimeTypeMapper::x11TypeToMime(atomName);

                                // Add unique MIME types
                                if (!mimeType.empty() && std::find(mimeTypes.begin(), mimeTypes.end(), mimeType) == mimeTypes.end()) {
                                    mimeTypes.push_back(mimeType);
                                }

                                // If atom name looks like a MIME type already, add it too
                                if (atomName.find("/") != std::string::npos && atomName != mimeType) {
                                    if (std::find(mimeTypes.begin(), mimeTypes.end(), atomName) == mimeTypes.end()) {
                                        mimeTypes.push_back(atomName);
                                    }
                                }
                            }
                        }
                    }

                    if (reply) {
                        free(reply);
                    }
                }
                received = true;
            }
            free(event);
        }

        // Clean up window
        xcb_destroy_window(mConnection, window);
        xcb_flush(mConnection);

        return mimeTypes;
    }

    std::string getAtomName(xcb_atom_t atom) const
    {
        xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(mConnection, atom);
        xcb_get_atom_name_reply_t *reply  = xcb_get_atom_name_reply(mConnection, cookie, nullptr);

        std::string name;
        if (reply) {
            char *atom_name = xcb_get_atom_name_name(reply);
            int length      = xcb_get_atom_name_name_length(reply);
            if (atom_name && length > 0) {
                name = std::string(atom_name, length);
            }
            free(reply);
        }
        return name;
    }

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

    void startEventLoop()
    {
        if (mIsRunning) {
            return; // Already running
        }

        mIsRunning         = true;
        mListeningForInput = false; // Start without input listening

        mListenerThread = std::thread([this]() {
            runEventLoop();
        });
    }

    void stopEventLoop()
    {
        if (!mIsRunning) {
            return;
        }

        mIsRunning         = false;
        mListeningForInput = false;

        // Wait for thread to exit
        if (mListenerThread.joinable()) {
            mListenerThread.join();
        }
    }

    void runEventLoop()
    {
        if (!mConnection) {
            mLogger.error("No connection in event loop");
            mIsRunning = false;
            return;
        }

        mLogger.debug("Starting event loop (always running for clipboard)...");

        bool currentInputListening = false;

        // Event loop
        while (mIsRunning) {
            // Check if input listening state changed
            if (currentInputListening != mListeningForInput) {
                currentInputListening = mListeningForInput;

                if (currentInputListening) {
                    // Enable XInput2 events
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

                    xcb_input_xi_select_events_checked(mConnection, mScreen->root, 1, &eventMask.header);
                    mLogger.debug("Enabled input event listening");
                } else {
                    // Disable XInput2 events
                    struct
                    {
                        xcb_input_event_mask_t header;
                        uint32_t mask;
                    } eventMask;

                    eventMask.header.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
                    eventMask.header.mask_len = 1;
                    eventMask.mask            = 0; // No events

                    xcb_input_xi_select_events_checked(mConnection, mScreen->root, 1, &eventMask.header);
                    mLogger.debug("Disabled input event listening");
                }
                xcb_flush(mConnection);
            }
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

            // Check if this is an XInput2 event (only process if listening for input)
            if (response_type == XCB_GE_GENERIC && mListeningForInput) {
                xcb_ge_generic_event_t *ge = reinterpret_cast<xcb_ge_generic_event_t *>(event);

                if (ge->extension == mXinputOpcode) {
                    processXInputEvent(ge);
                }
            }
            // Always handle selection requests for clipboard
            else if (response_type == XCB_SELECTION_REQUEST) {
                processSelectionRequest(reinterpret_cast<xcb_selection_request_event_t *>(event));
            }
            // Always monitor for desktop changes
            else if (response_type == XCB_CONFIGURE_NOTIFY) {
                processDesktopChangeEvent(reinterpret_cast<xcb_configure_notify_event_t *>(event));
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
                auto *motionEvent = reinterpret_cast<xcb_input_raw_motion_event_t *>(ge);
                event.type        = EventType::MouseMove;

                // Extract raw deltas from XInput2 raw motion event
                // The raw values are FP3232 (fixed point) values following the event structure
                // First comes the valuator mask, then raw values, then unaccelerated values
                uint32_t *valuator_mask = (uint32_t *)(&motionEvent[1]);
                int mask_len            = motionEvent->valuators_len;

                // Skip past the valuator mask to get to raw values
                xcb_input_fp3232_t *raw_values = (xcb_input_fp3232_t *)(valuator_mask + mask_len);

                // Extract deltas from valuators 0 (x) and 1 (y)
                int value_index = 0;
                for (int i = 0; i < 32 && value_index < 2; i++) {
                    if (valuator_mask[i / 32] & (1 << (i % 32))) {
                        if (value_index == 0) {
                            // X delta
                            event.state.dx = raw_values[value_index].integral;
                        } else if (value_index == 1) {
                            // Y delta
                            event.state.dy = raw_values[value_index].integral;
                        }
                        value_index++;
                    }
                }

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

    void processSelectionRequest(xcb_selection_request_event_t *req)
    {
        xcb_atom_t clipboard_atom = getAtom("CLIPBOARD");
        xcb_atom_t primary_atom   = getAtom("PRIMARY");
        xcb_atom_t utf8_atom      = getAtom("UTF8_STRING");
        xcb_atom_t text_atom      = getAtom("TEXT");
        xcb_atom_t targets_atom   = getAtom("TARGETS");

        xcb_selection_notify_event_t notify_event = {};
        notify_event.response_type                = XCB_SELECTION_NOTIFY;
        notify_event.time                         = req->time;
        notify_event.requestor                    = req->requestor;
        notify_event.selection                    = req->selection;
        notify_event.target                       = req->target;
        notify_event.property                     = XCB_NONE; // Default to refusing

        if ((req->selection == clipboard_atom || req->selection == primary_atom) &&
            (!mClipboardText.empty() || !mClipboardData.empty())) {
            if (req->target == targets_atom) {
                // Return list of supported targets
                std::vector<xcb_atom_t> targets;

                // Always include text targets if we have text
                if (!mClipboardText.empty()) {
                    targets.push_back(utf8_atom);
                    targets.push_back(text_atom);
                    targets.push_back(XCB_ATOM_STRING);
                }

                // Add MIME type targets for binary data
                for (const auto &pair : mClipboardData) {
                    xcb_atom_t mimeAtom = getAtom(pair.first.c_str());
                    if (mimeAtom != XCB_NONE) {
                        targets.push_back(mimeAtom);
                    }
                }

                if (!targets.empty()) {
                    xcb_change_property(mConnection, XCB_PROP_MODE_REPLACE, req->requestor, req->property, XCB_ATOM_ATOM, 32, targets.size(), targets.data());
                    notify_event.property = req->property;
                }
            } else if (req->target == utf8_atom || req->target == text_atom || req->target == XCB_ATOM_STRING) {
                // Return the clipboard text
                if (!mClipboardText.empty()) {
                    xcb_change_property(mConnection, XCB_PROP_MODE_REPLACE, req->requestor, req->property, req->target, 8, mClipboardText.length(), mClipboardText.c_str());
                    notify_event.property = req->property;
                }
            } else {
                // Check if it's a MIME type we have data for
                std::string targetName = getAtomName(req->target);
                auto it                = mClipboardData.find(targetName);
                if (it != mClipboardData.end()) {
                    xcb_change_property(mConnection, XCB_PROP_MODE_REPLACE, req->requestor, req->property, req->target, 8, it->second.size(), it->second.data());
                    notify_event.property = req->property;
                }
            }
        }

        xcb_send_event(mConnection, false, req->requestor, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<char *>(&notify_event));
        xcb_flush(mConnection);
    }

    void updateDesktopInfo()
    {
        Desktop newDesktop;

        // Query RandR for screen resources
        auto res_cookie = xcb_randr_get_screen_resources_current(mConnection, mScreen->root);
        auto res_reply  = xcb_randr_get_screen_resources_current_reply(mConnection, res_cookie, nullptr);

        if (!res_reply) {
            mLogger.error("Failed to get RandR screen resources");
            // Fallback to screen dimensions
            newDesktop.width  = mScreen->width_in_pixels;
            newDesktop.height = mScreen->height_in_pixels;

            std::lock_guard<std::mutex> lock(mDesktopMutex);
            mCurrentDesktop = newDesktop;
            return;
        }

        // Get CRTCs (display controllers)
        xcb_randr_crtc_t *crtcs = xcb_randr_get_screen_resources_current_crtcs(res_reply);
        int num_crtcs           = xcb_randr_get_screen_resources_current_crtcs_length(res_reply);

        int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
        bool first         = true;
        uint32_t displayId = 0;

        for (int i = 0; i < num_crtcs; i++) {
            auto crtc_cookie = xcb_randr_get_crtc_info(mConnection, crtcs[i], XCB_CURRENT_TIME);
            auto crtc_reply  = xcb_randr_get_crtc_info_reply(mConnection, crtc_cookie, nullptr);

            if (!crtc_reply) {
                continue;
            }

            // Skip disabled CRTCs
            if (crtc_reply->mode == XCB_NONE || crtc_reply->width == 0 || crtc_reply->height == 0) {
                free(crtc_reply);
                continue;
            }

            Display display;
            display.id        = displayId++;
            display.x         = crtc_reply->x;
            display.y         = crtc_reply->y;
            display.width     = crtc_reply->width;
            display.height    = crtc_reply->height;
            display.isPrimary = (i == 0); // First active CRTC is considered primary

            newDesktop.displays.push_back(display);

            // Update bounding box
            if (first) {
                minX  = display.x;
                minY  = display.y;
                maxX  = display.x + display.width;
                maxY  = display.y + display.height;
                first = false;
            } else {
                minX = std::min(minX, display.x);
                minY = std::min(minY, display.y);
                maxX = std::max(maxX, display.x + display.width);
                maxY = std::max(maxY, display.y + display.height);
            }

            free(crtc_reply);
        }

        free(res_reply);

        newDesktop.width  = maxX - minX;
        newDesktop.height = maxY - minY;

        // Check if desktop configuration changed
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mDesktopMutex);
            if (newDesktop.width != mCurrentDesktop.width ||
                newDesktop.height != mCurrentDesktop.height ||
                newDesktop.displays.size() != mCurrentDesktop.displays.size()) {
                mCurrentDesktop = newDesktop;
                changed         = true;
            }
        }

        if (changed && eventCallback) {
            mLogger.debug("Desktop changed: " + std::to_string(newDesktop.displays.size()) + " displays, " +
                          std::to_string(newDesktop.width) + "x" + std::to_string(newDesktop.height));

            Event event {};
            event.type      = EventType::DesktopChanged;
            event.timestamp = timestamp();
            event.state     = getState();

            eventCallback(event);
        }
    }

    void processDesktopChangeEvent(xcb_configure_notify_event_t *config_event)
    {
        // Only process events for the root window (desktop changes)
        if (config_event->window != mScreen->root) {
            return;
        }

        // Re-query display information when desktop changes
        updateDesktopInfo();
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
    std::atomic<bool> mListeningForInput { false };

    bool mCursorVisible { true };
    xcb_cursor_t mBlankCursor { XCB_NONE };

    // Desktop change monitoring
    mutable std::mutex mDesktopMutex;
    Desktop mCurrentDesktop;

    // Clipboard support
    mutable std::string mClipboardText;
    mutable std::unordered_map<std::string, std::vector<uint8_t>> mClipboardData; // MIME type -> data
    xcb_window_t mClipboardWindow { XCB_NONE };
};

// createPlatformHook moved to KonfliktNativeLinux.cpp for platform detection

} // namespace konflikt

#endif // __linux__
