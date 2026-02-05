#include "konflikt/WebSocketServer.h"

#include <App.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace konflikt {

// Template implementation that works for both SSL and non-SSL
template <bool SSL>
struct WebSocketServerImplT
{
    // User data for each connection
    struct PerSocketData
    {
        // Can add per-connection data here if needed
    };

    using WebSocket = uWS::WebSocket<SSL, true, PerSocketData>;
    using App = typename std::conditional<SSL, uWS::SSLApp, uWS::App>::type;

    std::thread serverThread;
    std::atomic<bool> running { false };
    us_listen_socket_t *listenSocket { nullptr };
    uWS::Loop *loop { nullptr };

    mutable std::mutex connectionsMutex;
    std::unordered_set<WebSocket *> connections;

    WebSocketServerCallbacks callbacks;
    WebSocketServerSSLConfig sslConfig;
    int port { 0 };

    void runWithApp(App &app, int requestedPort)
    {
        app.template ws<PerSocketData>("/ws", {
            .compression = uWS::DISABLED,
            .maxPayloadLength = 16 * 1024 * 1024,
            .idleTimeout = 120,
            .maxBackpressure = 1 * 1024 * 1024,

            .open = [this](WebSocket *ws) {
                {
                    std::lock_guard<std::mutex> lock(connectionsMutex);
                    connections.insert(ws);
                }
                if (callbacks.onConnect) {
                    callbacks.onConnect(static_cast<void *>(ws));
                }
            },

            .message = [this](WebSocket *ws, std::string_view message, uWS::OpCode /*opCode*/) {
                if (callbacks.onMessage) {
                    callbacks.onMessage(std::string(message), static_cast<void *>(ws));
                }
            },

            .close = [this](WebSocket *ws, int /*code*/, std::string_view /*message*/) {
                {
                    std::lock_guard<std::mutex> lock(connectionsMutex);
                    connections.erase(ws);
                }
                if (callbacks.onDisconnect) {
                    callbacks.onDisconnect(static_cast<void *>(ws));
                }
            }
        });

        app.listen(requestedPort, [this](us_listen_socket_t *socket) {
            if (socket) {
                listenSocket = socket;
                port = us_socket_local_port(SSL ? 1 : 0, reinterpret_cast<us_socket_t *>(socket));
                running = true;
            } else {
                // Failed to listen
                port = 0;
            }
        });

        loop = uWS::Loop::get();
        app.run();

        running = false;
    }

    void run(int requestedPort)
    {
        if constexpr (SSL) {
            uWS::SocketContextOptions options;
            options.key_file_name = sslConfig.keyFile.c_str();
            options.cert_file_name = sslConfig.certFile.c_str();
            if (!sslConfig.passphrase.empty()) {
                options.passphrase = sslConfig.passphrase.c_str();
            }
            App app(options);
            runWithApp(app, requestedPort);
        } else {
            App app;
            runWithApp(app, requestedPort);
        }
    }

    void sendMessage(void *connection, const std::string &message)
    {
        auto *ws = static_cast<WebSocket *>(connection);
        ws->send(message, uWS::OpCode::TEXT);
    }

    void broadcastMessage(const std::string &message)
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        for (auto *ws : connections) {
            ws->send(message, uWS::OpCode::TEXT);
        }
    }

    void closeAll()
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        for (auto *ws : connections) {
            ws->close();
        }
    }

    size_t getClientCount() const
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        return connections.size();
    }
};

// Type-erased implementation wrapper
struct WebSocketServer::Impl
{
    bool isSSL { false };

    // Use pointers to avoid instantiating both templates
    std::unique_ptr<WebSocketServerImplT<false>> nonSSL;
    std::unique_ptr<WebSocketServerImplT<true>> ssl;

    WebSocketServerCallbacks callbacks;
    WebSocketServerSSLConfig sslConfig;
    std::thread serverThread;
    std::atomic<bool> running { false };
    int port { 0 };

    void start(int requestedPort)
    {
        if (isSSL) {
            ssl = std::make_unique<WebSocketServerImplT<true>>();
            ssl->callbacks = callbacks;
            ssl->sslConfig = sslConfig;
            serverThread = std::thread([this, requestedPort]() {
                ssl->run(requestedPort);
            });

            // Wait for server to start
            while (!ssl->running && serverThread.joinable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (ssl->port == 0 && ssl->listenSocket == nullptr) {
                    break;
                }
            }
            running = ssl->running.load();
            port = ssl->port;
        } else {
            nonSSL = std::make_unique<WebSocketServerImplT<false>>();
            nonSSL->callbacks = callbacks;
            serverThread = std::thread([this, requestedPort]() {
                nonSSL->run(requestedPort);
            });

            // Wait for server to start
            while (!nonSSL->running && serverThread.joinable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (nonSSL->port == 0 && nonSSL->listenSocket == nullptr) {
                    break;
                }
            }
            running = nonSSL->running.load();
            port = nonSSL->port;
        }
    }

    void stop()
    {
        running = false;

        if (isSSL && ssl) {
            ssl->running = false;
            if (ssl->listenSocket) {
                us_listen_socket_close(1, ssl->listenSocket);
                ssl->listenSocket = nullptr;
            }
            if (ssl->loop) {
                ssl->loop->defer([this]() {
                    ssl->closeAll();
                });
            }
        } else if (nonSSL) {
            nonSSL->running = false;
            if (nonSSL->listenSocket) {
                us_listen_socket_close(0, nonSSL->listenSocket);
                nonSSL->listenSocket = nullptr;
            }
            if (nonSSL->loop) {
                nonSSL->loop->defer([this]() {
                    nonSSL->closeAll();
                });
            }
        }

        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

    void send(void *connection, const std::string &message)
    {
        if (isSSL && ssl) {
            ssl->sendMessage(connection, message);
        } else if (nonSSL) {
            nonSSL->sendMessage(connection, message);
        }
    }

    void broadcast(const std::string &message)
    {
        if (isSSL && ssl) {
            ssl->broadcastMessage(message);
        } else if (nonSSL) {
            nonSSL->broadcastMessage(message);
        }
    }

    size_t clientCount() const
    {
        if (isSSL && ssl) {
            return ssl->getClientCount();
        } else if (nonSSL) {
            return nonSSL->getClientCount();
        }
        return 0;
    }
};

WebSocketServer::WebSocketServer(int port)
    : mImpl(std::make_unique<Impl>())
    , mPort(port)
    , mSSLEnabled(false)
{
    mImpl->isSSL = false;
}

WebSocketServer::WebSocketServer(int port, const WebSocketServerSSLConfig &sslConfig)
    : mImpl(std::make_unique<Impl>())
    , mPort(port)
    , mSSLEnabled(true)
    , mSSLConfig(sslConfig)
{
    mImpl->isSSL = true;
    mImpl->sslConfig = sslConfig;
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

void WebSocketServer::setCallbacks(WebSocketServerCallbacks callbacks)
{
    mImpl->callbacks = std::move(callbacks);
}

bool WebSocketServer::start()
{
    if (mRunning) {
        return true;
    }

    mImpl->start(mPort);

    mRunning = mImpl->running;
    if (mRunning) {
        mPort = mImpl->port;
    }
    return mRunning;
}

void WebSocketServer::stop()
{
    if (!mRunning) {
        return;
    }

    mRunning = false;
    mImpl->stop();
}

void WebSocketServer::send(void *connection, const std::string &message)
{
    mImpl->send(connection, message);
}

void WebSocketServer::broadcast(const std::string &message)
{
    mImpl->broadcast(message);
}

size_t WebSocketServer::clientCount() const
{
    return mImpl->clientCount();
}

} // namespace konflikt
