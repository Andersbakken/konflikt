#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

namespace konflikt {

/// Callbacks for WebSocket server events
struct WebSocketServerCallbacks
{
    std::function<void(void *connection)> onConnect;
    std::function<void(void *connection)> onDisconnect;
    std::function<void(const std::string &message, void *connection)> onMessage;
};

/// WebSocket server using uWebSockets
class WebSocketServer
{
public:
    explicit WebSocketServer(int port);
    ~WebSocketServer();

    // Non-copyable
    WebSocketServer(const WebSocketServer &) = delete;
    WebSocketServer &operator=(const WebSocketServer &) = delete;

    /// Set callbacks
    void setCallbacks(WebSocketServerCallbacks callbacks);

    /// Start the server (non-blocking, runs in background)
    bool start();

    /// Stop the server
    void stop();

    /// Send message to a specific client
    void send(void *connection, const std::string &message);

    /// Broadcast message to all clients
    void broadcast(const std::string &message);

    /// Get the actual port (may differ if 0 was specified)
    int port() const { return m_port; }

    /// Check if server is running
    bool isRunning() const { return m_running; }

    /// Get number of connected clients
    size_t clientCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    int m_port;
    bool m_running { false };
    WebSocketServerCallbacks m_callbacks;
};

} // namespace konflikt
