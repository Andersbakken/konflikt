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

/// SSL/TLS configuration for WebSocket server
struct WebSocketServerSSLConfig
{
    std::string certFile;       // Path to certificate file (PEM)
    std::string keyFile;        // Path to private key file (PEM)
    std::string passphrase;     // Passphrase for encrypted key (optional)
};

/// WebSocket server using uWebSockets
class WebSocketServer
{
public:
    /// Create non-SSL server
    explicit WebSocketServer(int port);

    /// Create SSL server
    WebSocketServer(int port, const WebSocketServerSSLConfig &sslConfig);

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
    int port() const { return mPort; }

    /// Check if server is running
    bool isRunning() const { return mRunning; }

    /// Get number of connected clients
    size_t clientCount() const;

    /// Check if SSL is enabled
    bool isSSL() const { return mSSLEnabled; }

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;

    int mPort;
    bool mRunning { false };
    bool mSSLEnabled { false };
    WebSocketServerSSLConfig mSSLConfig;
    WebSocketServerCallbacks mCallbacks;
};

} // namespace konflikt
