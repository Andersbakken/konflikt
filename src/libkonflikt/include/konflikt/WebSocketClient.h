#pragma once

#include <functional>
#include <memory>
#include <string>

namespace konflikt {

/// Connection state
enum class WebSocketState
{
    Disconnected,
    Connecting,
    Connected,
    Error
};

/// Callbacks for WebSocket client events
struct WebSocketClientCallbacks
{
    std::function<void()> onConnect;
    std::function<void(const std::string &reason)> onDisconnect;
    std::function<void(const std::string &message)> onMessage;
    std::function<void(const std::string &error)> onError;
};

/// SSL/TLS configuration for WebSocket client
struct WebSocketClientSSLConfig
{
    std::string caFile;         // Path to CA certificate file (optional, for verification)
    bool verifyPeer { false };  // Whether to verify server certificate (disabled for self-signed)
};

/// WebSocket client using uWebSockets
class WebSocketClient
{
public:
    WebSocketClient();
    ~WebSocketClient();

    // Non-copyable
    WebSocketClient(const WebSocketClient &) = delete;
    WebSocketClient &operator=(const WebSocketClient &) = delete;

    /// Set callbacks
    void setCallbacks(WebSocketClientCallbacks callbacks);

    /// Enable TLS for connections
    void setSSL(const WebSocketClientSSLConfig &config);

    /// Connect to a server (ws:// or wss://)
    bool connect(const std::string &host, int port, const std::string &path = "/ws");

    /// Disconnect from server
    void disconnect();

    /// Send a message
    void send(const std::string &message);

    /// Get connection state
    WebSocketState state() const { return mState; }

    /// Check if connected
    bool isConnected() const { return mState == WebSocketState::Connected; }

    /// Process events (call this regularly or from event loop)
    void poll();

    /// Reconnect to the last server
    bool reconnect();

    /// Get last connected host
    const std::string &host() const { return mHost; }

    /// Get last connected port
    int port() const { return mPort; }

    /// Check if SSL is enabled
    bool isSSL() const { return mSSLEnabled; }

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;

    WebSocketState mState { WebSocketState::Disconnected };
    WebSocketClientCallbacks mCallbacks;
    std::string mHost;
    int mPort { 0 };
    std::string mPath;
    bool mSSLEnabled { false };
    WebSocketClientSSLConfig mSSLConfig;
};

} // namespace konflikt
