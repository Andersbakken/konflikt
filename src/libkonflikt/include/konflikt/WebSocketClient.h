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

    /// Connect to a server
    bool connect(const std::string &host, int port, const std::string &path = "/ws");

    /// Disconnect from server
    void disconnect();

    /// Send a message
    void send(const std::string &message);

    /// Get connection state
    WebSocketState state() const { return m_state; }

    /// Check if connected
    bool isConnected() const { return m_state == WebSocketState::Connected; }

    /// Process events (call this regularly or from event loop)
    void poll();

    /// Reconnect to the last server
    bool reconnect();

    /// Get last connected host
    const std::string &host() const { return m_host; }

    /// Get last connected port
    int port() const { return m_port; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    WebSocketState m_state { WebSocketState::Disconnected };
    WebSocketClientCallbacks m_callbacks;
    std::string m_host;
    int m_port { 0 };
    std::string m_path;
};

} // namespace konflikt
