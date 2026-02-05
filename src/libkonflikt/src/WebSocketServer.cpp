#include "konflikt/WebSocketServer.h"

#include <App.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace konflikt {

struct WebSocketServer::Impl
{
    // User data for each connection
    struct PerSocketData
    {
        // Can add per-connection data here if needed
    };

    using WebSocket = uWS::WebSocket<false, true, PerSocketData>;

    std::thread serverThread;
    std::atomic<bool> running { false };
    us_listen_socket_t *listenSocket { nullptr };
    uWS::Loop *loop { nullptr };

    std::mutex connectionsMutex;
    std::unordered_set<WebSocket *> connections;

    WebSocketServerCallbacks callbacks;
    int port { 0 };

    void run(int requestedPort)
    {
        uWS::App app;

        app.ws<PerSocketData>("/ws", { .compression = uWS::DISABLED, .maxPayloadLength = 16 * 1024 * 1024, .idleTimeout = 120, .maxBackpressure = 1 * 1024 * 1024,

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
        } });

        app.listen(requestedPort, [this, requestedPort](us_listen_socket_t *socket) {
            if (socket) {
                listenSocket = socket;
                port = us_socket_local_port(false, reinterpret_cast<us_socket_t *>(socket));
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
};

WebSocketServer::WebSocketServer(int port)
    : mImpl(std::make_unique<Impl>())
    , mPort(port)
{
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

    mImpl->serverThread = std::thread([this]() {
        mImpl->run(mPort);
    });

    // Wait for server to start
    while (!mImpl->running && mImpl->serverThread.joinable()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (mImpl->port == 0 && mImpl->listenSocket == nullptr) {
            // Server failed to start
            break;
        }
    }

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
    mImpl->running = false;

    // Close the listen socket to stop accepting new connections
    if (mImpl->listenSocket) {
        us_listen_socket_close(false, mImpl->listenSocket);
        mImpl->listenSocket = nullptr;
    }

    // Stop the event loop
    if (mImpl->loop) {
        mImpl->loop->defer([this]() {
            // Close all connections
            std::lock_guard<std::mutex> lock(mImpl->connectionsMutex);
            for (auto *ws : mImpl->connections) {
                ws->close();
            }
        });
    }

    if (mImpl->serverThread.joinable()) {
        mImpl->serverThread.join();
    }
}

void WebSocketServer::send(void *connection, const std::string &message)
{
    auto *ws = static_cast<Impl::WebSocket *>(connection);
    ws->send(message, uWS::OpCode::TEXT);
}

void WebSocketServer::broadcast(const std::string &message)
{
    std::lock_guard<std::mutex> lock(mImpl->connectionsMutex);
    for (auto *ws : mImpl->connections) {
        ws->send(message, uWS::OpCode::TEXT);
    }
}

size_t WebSocketServer::clientCount() const
{
    std::lock_guard<std::mutex> lock(mImpl->connectionsMutex);
    return mImpl->connections.size();
}

} // namespace konflikt
