#if defined(__linux__) && defined(WAYLAND_AVAILABLE)

#include "KonfliktNative.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <poll.h>
#include <thread>
#include <unordered_map>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

namespace konflikt {

class WaylandHook : public IPlatformHook
{
public:
    WaylandHook() = default;
    virtual ~WaylandHook() override = default;

    virtual bool initialize(const Logger &logger) override
    {
        mLogger = logger;
        
        // Connect to Wayland display
        mDisplay = wl_display_connect(nullptr);
        if (!mDisplay) {
            mLogger.error("Failed to connect to Wayland display");
            return false;
        }

        // Get registry and bind to interfaces
        mRegistry = wl_display_get_registry(mDisplay);
        if (!mRegistry) {
            mLogger.error("Failed to get Wayland registry");
            cleanup();
            return false;
        }

        wl_registry_add_listener(mRegistry, &mRegistryListener, this);
        
        // Process events to get initial interfaces
        wl_display_roundtrip(mDisplay);

        if (!mCompositor || !mShm) {
            mLogger.error("Required Wayland interfaces not available");
            cleanup();
            return false;
        }

        // Initialize current desktop state
        mCurrentDesktop.width = 1920;  // Default fallback
        mCurrentDesktop.height = 1080; // Default fallback

        mIsRunning = false;
        mListeningForInput = false;
        mCursorVisible = true;

        // Start the event loop immediately for monitoring
        startEventLoop();
        
        return true;
    }

    virtual void shutdown() override
    {
        stopEventLoop();
        cleanup();
    }

    virtual State getState() const override
    {
        State state {};
        
        // Wayland doesn't provide global cursor position without input focus
        // This is a security feature of Wayland
        state.x = 0;
        state.y = 0;
        
        // Mouse button and keyboard state would require seat input events
        state.mouseButtons = 0;
        state.keyboardModifiers = 0;
        
        return state;
    }

    virtual Desktop getDesktop() const override
    {
        std::lock_guard<std::mutex> lock(mDesktopMutex);
        return mCurrentDesktop;
    }

    virtual void sendMouseEvent(const Event &event) override
    {
        // Mouse event injection is not supported in Wayland for security reasons
        // This would require compositor-specific protocols or running as compositor
        mLogger.error("Mouse event injection not supported on Wayland");
    }

    virtual void sendKeyEvent(const Event &event) override
    {
        // Key event injection is not supported in Wayland for security reasons  
        // This would require compositor-specific protocols or running as compositor
        mLogger.error("Key event injection not supported on Wayland");
    }

    virtual void startListening() override
    {
        mListeningForInput = true;
        // Note: Input listening in Wayland requires seat capabilities
        // For now we just mark the flag, actual implementation would need seat events
        mLogger.debug("Started input listening (limited support on Wayland)");
    }

    virtual void stopListening() override
    {
        mListeningForInput = false;
        mLogger.debug("Stopped input listening");
    }

    virtual void showCursor() override
    {
        mCursorVisible = true;
        // Cursor visibility control requires surface focus in Wayland
    }

    virtual void hideCursor() override
    {
        mCursorVisible = false;
        // Cursor visibility control requires surface focus in Wayland
    }

    virtual bool isCursorVisible() const override
    {
        return mCursorVisible;
    }

    // Clipboard methods - Wayland uses different selection mechanisms
    virtual std::string getClipboardText(ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        // Wayland clipboard access requires data device manager and seat
        // For now return empty - would need full Wayland protocol implementation
        mLogger.debug("Clipboard text access not fully implemented for Wayland");
        return "";
    }

    virtual bool setClipboardText(const std::string &text, ClipboardSelection selection = ClipboardSelection::Auto) override
    {
        // Wayland clipboard access requires data device manager and seat
        mLogger.debug("Clipboard text setting not fully implemented for Wayland");
        return false;
    }

    virtual std::vector<uint8_t> getClipboardData(const std::string &mimeType, ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        // Wayland clipboard access requires data device manager and seat
        mLogger.debug("Clipboard data access not fully implemented for Wayland");
        return {};
    }

    virtual bool setClipboardData(const std::string &mimeType, const std::vector<uint8_t> &data, ClipboardSelection selection = ClipboardSelection::Auto) override
    {
        // Wayland clipboard access requires data device manager and seat
        mLogger.debug("Clipboard data setting not fully implemented for Wayland");
        return false;
    }

    virtual std::vector<std::string> getClipboardMimeTypes(ClipboardSelection selection = ClipboardSelection::Auto) const override
    {
        // Wayland clipboard access requires data device manager and seat
        mLogger.debug("Clipboard MIME types access not fully implemented for Wayland");
        return {};
    }

private:
    void cleanup()
    {
        if (mOutput) {
            wl_output_destroy(mOutput);
            mOutput = nullptr;
        }
        
        if (mDataDeviceManager) {
            wl_data_device_manager_destroy(mDataDeviceManager);
            mDataDeviceManager = nullptr;
        }
        
        if (mSeat) {
            wl_seat_destroy(mSeat);
            mSeat = nullptr;
        }
        
        if (mShm) {
            wl_shm_destroy(mShm);
            mShm = nullptr;
        }
        
        if (mCompositor) {
            wl_compositor_destroy(mCompositor);
            mCompositor = nullptr;
        }
        
        if (mRegistry) {
            wl_registry_destroy(mRegistry);
            mRegistry = nullptr;
        }
        
        if (mDisplay) {
            wl_display_disconnect(mDisplay);
            mDisplay = nullptr;
        }
    }

    void startEventLoop()
    {
        if (mIsRunning) {
            return;
        }

        mIsRunning = true;
        mListenerThread = std::thread([this]() {
            runEventLoop();
        });
    }

    void stopEventLoop()
    {
        if (!mIsRunning) {
            return;
        }

        mIsRunning = false;
        
        if (mListenerThread.joinable()) {
            mListenerThread.join();
        }
    }

    void runEventLoop()
    {
        if (!mDisplay) {
            mLogger.error("No display in event loop");
            mIsRunning = false;
            return;
        }

        mLogger.debug("Starting Wayland event loop...");

        while (mIsRunning) {
            // Dispatch pending events
            if (wl_display_dispatch_pending(mDisplay) == -1) {
                mLogger.error("Wayland display dispatch failed");
                break;
            }

            // Check for events with timeout
            struct pollfd pfd;
            pfd.fd = wl_display_get_fd(mDisplay);
            pfd.events = POLLIN;
            pfd.revents = 0;

            int ret = poll(&pfd, 1, 10); // 10ms timeout
            if (ret > 0) {
                if (wl_display_read_events(mDisplay) == -1) {
                    mLogger.error("Failed to read Wayland events");
                    break;
                }
            } else if (ret == -1) {
                mLogger.error("Poll error in Wayland event loop");
                break;
            }
            
            // Brief sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        mLogger.debug("Wayland event loop exited");
        mIsRunning = false;
    }

    // Wayland registry callbacks
    static void handleGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
    {
        auto *hook = static_cast<WaylandHook *>(data);
        
        if (strcmp(interface, wl_compositor_interface.name) == 0) {
            hook->mCompositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
        } else if (strcmp(interface, wl_shm_interface.name) == 0) {
            hook->mShm = static_cast<wl_shm*>(
                wl_registry_bind(registry, name, &wl_shm_interface, std::min(version, 1u)));
        } else if (strcmp(interface, wl_seat_interface.name) == 0) {
            hook->mSeat = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 5u)));
        } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
            hook->mDataDeviceManager = static_cast<wl_data_device_manager*>(
                wl_registry_bind(registry, name, &wl_data_device_manager_interface, std::min(version, 3u)));
        } else if (strcmp(interface, wl_output_interface.name) == 0) {
            hook->mOutput = static_cast<wl_output*>(
                wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 2u)));
            
            if (hook->mOutput) {
                wl_output_add_listener(hook->mOutput, &hook->mOutputListener, hook);
            }
        }
    }

    static void handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name)
    {
        // Handle interface removal if needed
    }

    // Output callbacks for desktop change detection
    static void handleOutputGeometry(void *data, struct wl_output *output, int32_t x, int32_t y,
                                   int32_t physical_width, int32_t physical_height, int32_t subpixel,
                                   const char *make, const char *model, int32_t transform)
    {
        // Geometry information received
    }

    static void handleOutputMode(void *data, struct wl_output *output, uint32_t flags,
                               int32_t width, int32_t height, int32_t refresh)
    {
        auto *hook = static_cast<WaylandHook *>(data);
        
        if (flags & WL_OUTPUT_MODE_CURRENT) {
            hook->updateDesktop(width, height);
        }
    }

    static void handleOutputDone(void *data, struct wl_output *output)
    {
        // Output information is complete
    }

    static void handleOutputScale(void *data, struct wl_output *output, int32_t factor)
    {
        // Scale factor received
    }

    void updateDesktop(int32_t width, int32_t height)
    {
        Desktop newDesktop;
        newDesktop.width = width;
        newDesktop.height = height;
        
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

    // Wayland objects
    struct wl_display *mDisplay { nullptr };
    struct wl_registry *mRegistry { nullptr };
    struct wl_compositor *mCompositor { nullptr };
    struct wl_shm *mShm { nullptr };
    struct wl_seat *mSeat { nullptr };
    struct wl_data_device_manager *mDataDeviceManager { nullptr };
    struct wl_output *mOutput { nullptr };

    // Event loop
    std::thread mListenerThread;
    std::atomic<bool> mIsRunning { false };
    std::atomic<bool> mListeningForInput { false };

    Logger mLogger;
    bool mCursorVisible { true };

    // Desktop change monitoring
    mutable std::mutex mDesktopMutex;
    Desktop mCurrentDesktop;

    // Wayland listeners
    const struct wl_registry_listener mRegistryListener = {
        handleGlobal,
        handleGlobalRemove
    };

    const struct wl_output_listener mOutputListener = {
        handleOutputGeometry,
        handleOutputMode,
        handleOutputDone,
        handleOutputScale
    };
};

} // namespace konflikt

#endif // defined(__linux__) && defined(WAYLAND_AVAILABLE)