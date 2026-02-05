#pragma once

#include "Platform.h"
#include "Protocol.h"
#include "Rect.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace konflikt {

// Forward declarations
class WebSocketServer;
class WebSocketClient;
class HttpServer;
class LayoutManager;
class ServiceDiscovery;
struct DiscoveredService;

/// Instance role
enum class InstanceRole
{
    Server,
    Client
};

/// Configuration for Konflikt instance
struct Config
{
    InstanceRole role { InstanceRole::Client };
    std::string instanceId;
    std::string instanceName;

    // Server settings
    int port { 3000 };

    // Client settings
    std::string serverHost;
    int serverPort { 3000 };

    // Screen settings
    int32_t screenX { 0 };
    int32_t screenY { 0 };
    int32_t screenWidth { 0 };  // 0 = auto-detect
    int32_t screenHeight { 0 }; // 0 = auto-detect

    // Screen edge transition settings (which edges trigger screen transitions)
    bool edgeLeft { true };
    bool edgeRight { true };
    bool edgeTop { true };
    bool edgeBottom { true };

    // Lock cursor to current screen (disable transitions)
    bool lockCursorToScreen { false };

    // Hotkey for toggling cursor lock (keycode, 0 = disabled)
    // Default: Scroll Lock key (macOS: 107, Linux: 78)
    uint32_t lockCursorHotkey { 107 };

    // UI settings
    std::string uiPath;      // Path to React UI files

    // Security/TLS settings
    bool useTLS { false };           // Enable WSS (WebSocket Secure)
    std::string tlsCertFile;         // Path to TLS certificate file (PEM)
    std::string tlsKeyFile;          // Path to TLS private key file (PEM)
    std::string tlsKeyPassphrase;    // Passphrase for encrypted key (optional)

    // Logging
    bool verbose { false };
    std::string logFile;

    // Debug API (enables /api/log endpoint)
    bool enableDebugApi { false };

    // Key remapping: map of source keycode -> target keycode
    // Applied when sending input events to clients
    // Example: {"55": 133} maps Mac Command Left (55) to Linux Super Left (133)
    // Common mappings:
    //   Mac Command Left (55) <-> Linux Super Left (133)
    //   Mac Command Right (54) <-> Linux Super Right (134)
    //   Mac Option Left (58) <-> Linux Alt Left (64)
    //   Mac Option Right (61) <-> Linux Alt Right (108)
    std::unordered_map<uint32_t, uint32_t> keyRemap;

    // Log keycodes for debugging key remapping (shows pressed keycodes in log)
    bool logKeycodes { false };
};

/// Connection status
enum class ConnectionStatus
{
    Disconnected,
    Connecting,
    Connected,
    Error
};

/// Callback types
using StatusCallback = std::function<void(ConnectionStatus status, const std::string &message)>;
using LogCallback = std::function<void(const std::string &level, const std::string &message)>;

/// Main Konflikt application class
class Konflikt
{
public:
    explicit Konflikt(const Config &config);
    ~Konflikt();

    // Non-copyable, non-movable
    Konflikt(const Konflikt &) = delete;
    Konflikt &operator=(const Konflikt &) = delete;
    Konflikt(Konflikt &&) = delete;
    Konflikt &operator=(Konflikt &&) = delete;

    /// Initialize the instance
    bool init();

    /// Run the main event loop (blocking)
    void run();

    /// Stop the instance
    void stop();

    /// Request quit
    void quit();

    /// Notify clients of graceful shutdown (server only)
    void notifyShutdown(const std::string &reason = "shutdown", int32_t delayMs = 0);

    /// Lock/unlock cursor to current screen
    void setLockCursorToScreen(bool locked);
    bool isLockCursorToScreen() const { return mConfig.lockCursorToScreen; }

    /// Set which screen edges trigger transitions
    void setEdgeTransitions(bool left, bool right, bool top, bool bottom);
    bool edgeLeft() const { return mConfig.edgeLeft; }
    bool edgeRight() const { return mConfig.edgeRight; }
    bool edgeTop() const { return mConfig.edgeTop; }
    bool edgeBottom() const { return mConfig.edgeBottom; }

    /// Get current configuration
    const Config &config() const { return mConfig; }

    /// Save current configuration to file
    bool saveConfig(const std::string &path = "");

    /// Get instance role
    InstanceRole role() const { return mConfig.role; }

    /// Get connection status
    ConnectionStatus connectionStatus() const { return mConnectionStatus; }

    /// Get connected server name (for clients)
    const std::string &connectedServerName() const { return mConnectedServerName; }

    /// Set status callback
    void setStatusCallback(StatusCallback callback) { mStatusCallback = std::move(callback); }

    /// Set log callback
    void setLogCallback(LogCallback callback) { mLogCallback = std::move(callback); }

    /// Get the HTTP server port (may differ from config if auto-assigned)
    int httpPort() const;

    /// Get the number of connected clients (server only)
    size_t clientCount() const { return mConnectedClients.size(); }

    /// Get the names of connected clients (server only)
    std::vector<std::string> connectedClientNames() const;

private:
    // Event handlers
    void onPlatformEvent(const Event &event);
    void onWebSocketMessage(const std::string &message, void *connection);
    void onClientConnected(void *connection);
    void onClientDisconnected(void *connection);

    // Message handlers
    void handleHandshakeRequest(const HandshakeRequest &request, void *connection);
    void handleHandshakeResponse(const HandshakeResponse &response);
    void handleInputEvent(const InputEventMessage &message);
    void handleClientRegistration(const ClientRegistrationMessage &message);
    void handleLayoutAssignment(const LayoutAssignmentMessage &message);
    void handleLayoutUpdate(const LayoutUpdateMessage &message);
    void handleActivateClient(const ActivateClientMessage &message);
    void handleDeactivationRequest(const DeactivationRequestMessage &message);
    void handleClipboardSync(const ClipboardSyncMessage &message);
    void handleServerShutdown(const ServerShutdownMessage &message);

    // Clipboard
    void checkClipboardChange();
    void broadcastClipboard(const std::string &text);

    // Service discovery
    void onServiceFound(const DiscoveredService &service);
    void onServiceLost(const std::string &name);
    void connectToDiscoveredServer(const std::string &host, int port);

    // Screen transition
    bool checkScreenTransition(int32_t x, int32_t y);
    void activateClient(const std::string &targetInstanceId, int32_t cursorX, int32_t cursorY);
    void deactivateRemoteScreen();
    void requestDeactivation();

    // Broadcasting
    void broadcastInputEvent(const std::string &eventType, const InputEventData &data);
    void broadcastToClients(const std::string &message);

    // Utility
    void updateStatus(ConnectionStatus status, const std::string &message);
    void log(const std::string &level, const std::string &message);
    std::string generateMachineId();
    std::string generateDisplayId();
    uint32_t remapKeycode(uint32_t keycode) const;

    // Configuration
    Config mConfig;

    // Platform
    std::unique_ptr<IPlatform> mPlatform;
    Logger mLogger;

    // Networking
    std::unique_ptr<WebSocketServer> mWsServer;
    std::unique_ptr<WebSocketClient> mWsClient;
    std::unique_ptr<HttpServer> mHttpServer;
    std::unique_ptr<ServiceDiscovery> mServiceDiscovery;

    // Layout
    std::unique_ptr<LayoutManager> mLayoutManager;

    // State
    bool mRunning { false };
    uint64_t mStartTime { 0 };
    ConnectionStatus mConnectionStatus { ConnectionStatus::Disconnected };
    std::string mConnectedServerName;
    bool mIsActiveInstance { false };
    Rect mScreenBounds;

    // Virtual cursor for remote screen control
    struct
    {
        int32_t x {};
        int32_t y {};
    } mVirtualCursor;

    bool mHasVirtualCursor { false };
    Rect mActiveRemoteScreenBounds;

    // Client tracking
    std::string mActivatedClientId;
    std::string mMachineId;
    std::string mDisplayId;
    uint64_t mLastDeactivationTime { 0 };
    uint64_t mLastDeactivationRequest { 0 };

    // Client connection tracking (for server)
    struct ConnectedClient
    {
        std::string instanceId;
        std::string displayName;
        int32_t screenWidth {};
        int32_t screenHeight {};
        uint64_t connectedAt {};
        bool active { false };  // Currently receiving input
    };
    std::unordered_map<void *, std::string> mConnectionToInstanceId;
    std::unordered_map<std::string, ConnectedClient> mConnectedClients;

    // Clipboard sync
    std::string mLastClipboardText;
    uint32_t mClipboardSequence { 0 };
    uint64_t mLastClipboardCheck { 0 };

    // Reconnection
    uint64_t mLastReconnectAttempt { 0 };
    int mReconnectAttempts { 0 };
    bool mExpectingReconnect { false };  // Set when server sent graceful shutdown
    int32_t mExpectedRestartDelayMs { 0 };
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr uint64_t RECONNECT_DELAY_MS = 3000;

    // Callbacks
    StatusCallback mStatusCallback;
    LogCallback mLogCallback;

    // Log buffer for debug API
    struct LogEntry
    {
        std::string timestamp;
        std::string level;
        std::string message;
    };
    std::vector<LogEntry> mLogBuffer;
    static constexpr size_t MAX_LOG_ENTRIES = 500;
    mutable std::mutex mLogBufferMutex;

    // Input event statistics
    struct InputStats
    {
        uint64_t totalEvents { 0 };
        uint64_t mouseEvents { 0 };
        uint64_t keyEvents { 0 };
        uint64_t scrollEvents { 0 };
        uint64_t windowStartTime { 0 };
        uint64_t eventsInWindow { 0 };
        double eventsPerSecond { 0.0 };
        // Latency tracking (client-side only, measures event timestamp to execution)
        double lastLatencyMs { 0.0 };
        double avgLatencyMs { 0.0 };
        double maxLatencyMs { 0.0 };
        uint64_t latencySamples { 0 };
        double latencySum { 0.0 };
    };
    InputStats mInputStats;
    void updateInputStats(const std::string &eventType);
    void recordLatency(uint64_t eventTimestamp);
};

} // namespace konflikt
