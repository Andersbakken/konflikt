#pragma once

#include "Platform.h"
#include "Protocol.h"
#include "Rect.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

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

    // UI settings
    std::string uiPath;      // Path to React UI files

    // Logging
    bool verbose { false };
    std::string logFile;
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

    /// Get current configuration
    const Config &config() const { return m_config; }

    /// Get instance role
    InstanceRole role() const { return m_config.role; }

    /// Get connection status
    ConnectionStatus connectionStatus() const { return m_connectionStatus; }

    /// Get connected server name (for clients)
    const std::string &connectedServerName() const { return m_connectedServerName; }

    /// Set status callback
    void setStatusCallback(StatusCallback callback) { m_statusCallback = std::move(callback); }

    /// Set log callback
    void setLogCallback(LogCallback callback) { m_logCallback = std::move(callback); }

    /// Get the HTTP server port (may differ from config if auto-assigned)
    int httpPort() const;

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

    // Configuration
    Config m_config;

    // Platform
    std::unique_ptr<IPlatform> m_platform;
    Logger m_logger;

    // Networking
    std::unique_ptr<WebSocketServer> m_wsServer;
    std::unique_ptr<WebSocketClient> m_wsClient;
    std::unique_ptr<HttpServer> m_httpServer;
    std::unique_ptr<ServiceDiscovery> m_serviceDiscovery;

    // Layout
    std::unique_ptr<LayoutManager> m_layoutManager;

    // State
    bool m_running { false };
    ConnectionStatus m_connectionStatus { ConnectionStatus::Disconnected };
    std::string m_connectedServerName;
    bool m_isActiveInstance { false };
    Rect m_screenBounds;

    // Virtual cursor for remote screen control
    struct
    {
        int32_t x {};
        int32_t y {};
    } m_virtualCursor;

    bool m_hasVirtualCursor { false };
    Rect m_activeRemoteScreenBounds;

    // Client tracking
    std::string m_activatedClientId;
    std::string m_machineId;
    std::string m_displayId;
    uint64_t m_lastDeactivationTime { 0 };
    uint64_t m_lastDeactivationRequest { 0 };

    // Client connection tracking (for server)
    std::unordered_map<void *, std::string> m_connectionToInstanceId;

    // Clipboard sync
    std::string m_lastClipboardText;
    uint32_t m_clipboardSequence { 0 };
    uint64_t m_lastClipboardCheck { 0 };

    // Reconnection
    uint64_t m_lastReconnectAttempt { 0 };
    int m_reconnectAttempts { 0 };
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr uint64_t RECONNECT_DELAY_MS = 3000;

    // Callbacks
    StatusCallback m_statusCallback;
    LogCallback m_logCallback;
};

} // namespace konflikt
