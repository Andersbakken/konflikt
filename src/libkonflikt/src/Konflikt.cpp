#include "konflikt/Konflikt.h"
#include "konflikt/ConfigManager.h"
#include "konflikt/HttpServer.h"
#include "konflikt/LayoutManager.h"
#include "konflikt/ServiceDiscovery.h"
#include "konflikt/Version.h"
#include "konflikt/WebSocketClient.h"
#include "konflikt/WebSocketServer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <openssl/sha.h>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace konflikt {

Konflikt::Konflikt(const Config &config)
    : mConfig(config)
{
    // Generate identifiers
    mMachineId = generateMachineId();

    // Auto-generate instanceId if not provided
    if (mConfig.instanceId.empty()) {
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        // Use first 8 chars of machine ID + hostname for readability
        mConfig.instanceId = std::string(hostname) + "-" + mMachineId.substr(0, 8);
    }

    // Auto-generate instanceName if not provided
    if (mConfig.instanceName.empty()) {
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        mConfig.instanceName = hostname;
    }
}

Konflikt::~Konflikt()
{
    stop();
}

bool Konflikt::init()
{
    // Set up logger
    mLogger.verbose = [this](const std::string &msg) {
        log("verbose", msg);
    };
    mLogger.debug = [this](const std::string &msg) {
        log("debug", msg);
    };
    mLogger.log = [this](const std::string &msg) {
        log("log", msg);
    };
    mLogger.error = [this](const std::string &msg) {
        log("error", msg);
    };

    // Create platform
    mPlatform = createPlatform();
    if (!mPlatform || !mPlatform->initialize(mLogger)) {
        log("error", "Failed to initialize platform");
        return false;
    }

    // Get screen bounds
    Desktop desktop = mPlatform->getDesktop();
    mScreenBounds = Rect(
        mConfig.screenX,
        mConfig.screenY,
        mConfig.screenWidth > 0 ? mConfig.screenWidth : desktop.width,
        mConfig.screenHeight > 0 ? mConfig.screenHeight : desktop.height);

    mDisplayId = generateDisplayId();

    log("log", "Screen bounds: " + std::to_string(mScreenBounds.width) + "x" + std::to_string(mScreenBounds.height));

    // Create WebSocket server (with optional TLS)
    if (mConfig.useTLS && !mConfig.tlsCertFile.empty() && !mConfig.tlsKeyFile.empty()) {
        WebSocketServerSSLConfig sslConfig;
        sslConfig.certFile = mConfig.tlsCertFile;
        sslConfig.keyFile = mConfig.tlsKeyFile;
        sslConfig.passphrase = mConfig.tlsKeyPassphrase;
        mWsServer = std::make_unique<WebSocketServer>(mConfig.port, sslConfig);
        log("log", "TLS enabled for WebSocket server");
    } else {
        mWsServer = std::make_unique<WebSocketServer>(mConfig.port);
    }
    mWsServer->setCallbacks({ .onConnect = [this](void *conn) {
        onClientConnected(conn);
    }, .onDisconnect = [this](void *conn) {
        onClientDisconnected(conn);
    }, .onMessage = [this](const std::string &msg, void *conn) {
        onWebSocketMessage(msg, conn);
    } });

    // Create HTTP server (same port as WebSocket for now)
    mHttpServer = std::make_unique<HttpServer>(mConfig.port);

    // Serve static UI files if path is configured
    if (!mConfig.uiPath.empty() && std::filesystem::exists(mConfig.uiPath)) {
        mHttpServer->serveStatic("/ui/", mConfig.uiPath);
        log("log", "Serving UI from " + mConfig.uiPath);
    }

    // API endpoint for version
    mHttpServer->route("GET", "/api/version", [](const HttpRequest &) {
        HttpResponse response;
        response.contentType = "application/json";
        response.body = "{\"version\":\"" + std::string(VERSION) + "\"}";
        return response;
    });

    // API endpoint for server info (including TLS availability)
    mHttpServer->route("GET", "/api/server-info", [this](const HttpRequest &) {
        HttpResponse response;
        response.contentType = "application/json";
        response.body = "{\"name\":\"" + mConfig.instanceName + "\","
                       "\"port\":" + std::to_string(mConfig.port) + ","
                       "\"tls\":" + (mConfig.useTLS ? "true" : "false") + "}";
        return response;
    });

    // API endpoint to download server certificate (for TLS trust)
    if (mConfig.useTLS && !mConfig.tlsCertFile.empty()) {
        mHttpServer->route("GET", "/api/cert", [this](const HttpRequest &) {
            HttpResponse response;

            // Read certificate file
            std::ifstream certFile(mConfig.tlsCertFile);
            if (certFile.is_open()) {
                std::stringstream buffer;
                buffer << certFile.rdbuf();
                response.body = buffer.str();
                response.contentType = "application/x-pem-file";
                response.headers["Content-Disposition"] = "attachment; filename=\"konflikt-server.crt\"";
            } else {
                response.statusCode = 404;
                response.statusMessage = "Not Found";
                response.body = "Certificate not available";
            }

            return response;
        });
        log("log", "Certificate available at /api/cert");
    }

    // API endpoint for server status
    mHttpServer->route("GET", "/api/status", [this](const HttpRequest &) {
        HttpResponse response;
        response.contentType = "application/json";

        std::stringstream ss;
        ss << "{";
        ss << "\"version\":\"" << VERSION << "\",";
        ss << "\"role\":\"" << (mConfig.role == InstanceRole::Server ? "server" : "client") << "\",";
        ss << "\"instanceId\":\"" << mConfig.instanceId << "\",";
        ss << "\"instanceName\":\"" << mConfig.instanceName << "\",";
        ss << "\"status\":\"" << (mRunning ? "running" : "stopped") << "\",";
        ss << "\"connection\":\"";
        switch (mConnectionStatus) {
            case ConnectionStatus::Connected: ss << "connected"; break;
            case ConnectionStatus::Connecting: ss << "connecting"; break;
            case ConnectionStatus::Disconnected: ss << "disconnected"; break;
            case ConnectionStatus::Error: ss << "error"; break;
        }
        ss << "\",";

        if (mConfig.role == InstanceRole::Server && mWsServer) {
            ss << "\"clientCount\":" << mWsServer->clientCount() << ",";
            ss << "\"tls\":" << (mConfig.useTLS ? "true" : "false") << ",";
            ss << "\"port\":" << mWsServer->port() << ",";
            ss << "\"activeClient\":\"" << mActivatedClientId << "\",";
            ss << "\"clients\":[";
            bool first = true;
            for (const auto &[id, client] : mConnectedClients) {
                if (!first) {
                    ss << ",";
                }
                first = false;
                ss << "{";
                ss << "\"instanceId\":\"" << client.instanceId << "\",";
                ss << "\"displayName\":\"" << client.displayName << "\",";
                ss << "\"screenWidth\":" << client.screenWidth << ",";
                ss << "\"screenHeight\":" << client.screenHeight << ",";
                ss << "\"connectedAt\":" << client.connectedAt << ",";
                ss << "\"active\":" << (client.active ? "true" : "false");
                ss << "}";
            }
            ss << "]";
        } else {
            ss << "\"serverHost\":\"" << mConfig.serverHost << "\",";
            ss << "\"serverPort\":" << mConfig.serverPort << ",";
            ss << "\"connectedServer\":\"" << mConnectedServerName << "\"";
        }

        ss << "}";
        response.body = ss.str();
        return response;
    });

    // API endpoint for recent logs (only if debug API is enabled)
    if (mConfig.enableDebugApi) {
        mHttpServer->route("GET", "/api/log", [this](const HttpRequest &) {
            HttpResponse response;
            response.contentType = "application/json";

            std::stringstream ss;
            ss << "{\"logs\":[";

            {
                std::lock_guard<std::mutex> lock(mLogBufferMutex);
                bool first = true;
                for (const auto &entry : mLogBuffer) {
                    if (!first) {
                        ss << ",";
                    }
                    first = false;
                    ss << "{\"timestamp\":\"" << entry.timestamp << "\",";
                    ss << "\"level\":\"" << entry.level << "\",";
                    // Escape quotes in message
                    ss << "\"message\":\"";
                    for (char c : entry.message) {
                        if (c == '"') {
                            ss << "\\\"";
                        } else if (c == '\\') {
                            ss << "\\\\";
                        } else if (c == '\n') {
                            ss << "\\n";
                        } else {
                            ss << c;
                        }
                    }
                    ss << "\"}";
                }
            }

            ss << "]}";
            response.body = ss.str();
            return response;
        });
        log("log", "Debug API enabled at /api/log");
    }

    // Set up platform event handler for server role
    if (mConfig.role == InstanceRole::Server) {
        mLayoutManager = std::make_unique<LayoutManager>();
        mLayoutManager->setServerScreen(
            mConfig.instanceId,
            mConfig.instanceName,
            mMachineId,
            mScreenBounds.width,
            mScreenBounds.height);

        mPlatform->onEvent = [this](const Event &event) {
            onPlatformEvent(event);
        };

        mPlatform->startListening();
        mIsActiveInstance = true;
    } else {
        // Client role: create WebSocket client
        mWsClient = std::make_unique<WebSocketClient>();

        // Enable TLS if configured
        if (mConfig.useTLS) {
            WebSocketClientSSLConfig sslConfig;
            // For self-signed certs, we don't verify by default
            sslConfig.verifyPeer = false;
            mWsClient->setSSL(sslConfig);
            log("log", "TLS enabled for WebSocket client");
        }

        mWsClient->setCallbacks({ .onConnect = [this]() {
            updateStatus(ConnectionStatus::Connected, "Connected to server");
            mReconnectAttempts = 0;       // Reset on successful connection
            mExpectingReconnect = false;  // Clear graceful shutdown flag
            mExpectedRestartDelayMs = 0;
                // Send handshake
            HandshakeRequest req;
            req.instanceId = mConfig.instanceId;
            req.instanceName = mConfig.instanceName;
            req.version = VERSION;
            req.capabilities = { "input_events", "screen_info" };
            req.timestamp = timestamp();
            mWsClient->send(toJson(req));
        }, .onDisconnect = [this](const std::string &reason) {
            updateStatus(ConnectionStatus::Disconnected, reason);
            // Trigger reconnection attempt
            mLastReconnectAttempt = 0; // Allow immediate first attempt
        }, .onMessage = [this](const std::string &msg) {
            onWebSocketMessage(msg, nullptr);
        }, .onError = [this](const std::string &err) {
            updateStatus(ConnectionStatus::Error, err);
        } });
    }

    // Initialize service discovery
    mServiceDiscovery = std::make_unique<ServiceDiscovery>();
    mServiceDiscovery->setCallbacks({
        .onServiceFound = [this](const DiscoveredService &service) {
            onServiceFound(service);
        },
        .onServiceLost = [this](const std::string &name) {
            onServiceLost(name);
        },
        .onError = [this](const std::string &error) {
            log("error", "Service discovery: " + error);
        }
    });

    return true;
}

void Konflikt::run()
{
    mRunning = true;

    // Start servers
    if (mConfig.role == InstanceRole::Server) {
        if (!mWsServer->start()) {
            log("error", "Failed to start WebSocket server");
            return;
        }
        if (!mHttpServer->start()) {
            log("error", "Failed to start HTTP server");
            return;
        }
        log("log", "Server listening on port " + std::to_string(mWsServer->port()));
        updateStatus(ConnectionStatus::Connected, "Server running");

        // Register service for discovery
        if (mServiceDiscovery->registerService(mConfig.instanceName, mWsServer->port(), mConfig.instanceId)) {
            log("log", "Registered mDNS service: " + mConfig.instanceName);
        }
    } else {
        // Client: connect to server
        if (!mConfig.serverHost.empty()) {
            log("log", "Connecting to " + mConfig.serverHost + ":" + std::to_string(mConfig.serverPort));
            updateStatus(ConnectionStatus::Connecting, "Connecting...");
            mWsClient->connect(mConfig.serverHost, mConfig.serverPort, "/ws");
        } else {
            // No server specified, browse for servers
            log("log", "Browsing for Konflikt servers...");
            updateStatus(ConnectionStatus::Connecting, "Searching for servers...");
            mServiceDiscovery->startBrowsing();
        }
    }

    // Main loop
    while (mRunning) {
        if (mWsClient) {
            mWsClient->poll();

            // Auto-reconnect for clients
            if (mConfig.role == InstanceRole::Client &&
                mConnectionStatus == ConnectionStatus::Disconnected &&
                !mWsClient->host().empty() &&
                mReconnectAttempts < MAX_RECONNECT_ATTEMPTS) {

                uint64_t now = timestamp();
                // Use shorter delay if server sent graceful shutdown, or use expected restart delay
                uint64_t delay = RECONNECT_DELAY_MS;
                if (mExpectingReconnect) {
                    // If server told us when it expects to be back, use that + small buffer
                    // Otherwise use a shorter delay for graceful restarts
                    delay = mExpectedRestartDelayMs > 0
                        ? static_cast<uint64_t>(mExpectedRestartDelayMs) + 500
                        : 1000;
                }

                if (now - mLastReconnectAttempt >= delay) {
                    mLastReconnectAttempt = now;
                    mReconnectAttempts++;
                    if (mExpectingReconnect) {
                        log("log", "Reconnecting after graceful server shutdown (attempt " +
                            std::to_string(mReconnectAttempts) + ")");
                    } else {
                        log("log", "Reconnection attempt " + std::to_string(mReconnectAttempts) +
                            "/" + std::to_string(MAX_RECONNECT_ATTEMPTS));
                    }
                    updateStatus(ConnectionStatus::Connecting, "Reconnecting...");
                    mWsClient->reconnect();
                }
            }
        }

        // Poll service discovery for events
        if (mServiceDiscovery) {
            mServiceDiscovery->poll();
        }

        // Check for clipboard changes periodically
        checkClipboardChange();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Konflikt::stop()
{
    mRunning = false;

    if (mPlatform) {
        mPlatform->stopListening();
        mPlatform->shutdown();
    }

    if (mWsServer) {
        mWsServer->stop();
    }

    if (mHttpServer) {
        mHttpServer->stop();
    }

    if (mWsClient) {
        mWsClient->disconnect();
    }
}

void Konflikt::quit()
{
    mRunning = false;
}

int Konflikt::httpPort() const
{
    return mHttpServer ? mHttpServer->port() : mConfig.port;
}

std::vector<std::string> Konflikt::connectedClientNames() const
{
    std::vector<std::string> names;
    for (const auto &[id, client] : mConnectedClients) {
        names.push_back(client.displayName);
    }
    return names;
}

uint32_t Konflikt::remapKeycode(uint32_t keycode) const
{
    auto it = mConfig.keyRemap.find(keycode);
    if (it != mConfig.keyRemap.end()) {
        return it->second;
    }
    return keycode;
}

void Konflikt::onPlatformEvent(const Event &event)
{
    switch (event.type) {
        case EventType::MouseMove: {
            // Update local cursor position
            if (mHasVirtualCursor && mActivatedClientId.empty() == false) {
                // Update virtual cursor
                int32_t newX = mVirtualCursor.x + event.state.dx;
                int32_t newY = mVirtualCursor.y + event.state.dy;

                mVirtualCursor.x = std::clamp(newX, 0, mActiveRemoteScreenBounds.width - 1);
                mVirtualCursor.y = std::clamp(newY, 0, mActiveRemoteScreenBounds.height - 1);

                InputEventData data;
                data.x = mVirtualCursor.x;
                data.y = mVirtualCursor.y;
                data.dx = event.state.dx;
                data.dy = event.state.dy;
                data.timestamp = event.timestamp;
                data.keyboardModifiers = event.state.keyboardModifiers;
                data.mouseButtons = event.state.mouseButtons;

                broadcastInputEvent("mouseMove", data);
            } else {
                // Check for screen transition
                if (checkScreenTransition(event.state.x, event.state.y)) {
                    return;
                }
            }
            break;
        }

        case EventType::MousePress:
        case EventType::MouseRelease: {
            if (mHasVirtualCursor) {
                InputEventData data;
                data.x = mVirtualCursor.x;
                data.y = mVirtualCursor.y;
                data.timestamp = event.timestamp;
                data.keyboardModifiers = event.state.keyboardModifiers;
                data.mouseButtons = event.state.mouseButtons;

                if (event.button == MouseButton::Left)
                    data.button = "left";
                else if (event.button == MouseButton::Right)
                    data.button = "right";
                else if (event.button == MouseButton::Middle)
                    data.button = "middle";

                broadcastInputEvent(event.type == EventType::MousePress ? "mousePress" : "mouseRelease", data);
            }
            break;
        }

        case EventType::KeyPress:
        case EventType::KeyRelease: {
            // Check for hotkey (only on key press)
            if (event.type == EventType::KeyPress && mConfig.lockCursorHotkey != 0 &&
                event.keycode == mConfig.lockCursorHotkey) {
                // Toggle cursor lock
                setLockCursorToScreen(!mConfig.lockCursorToScreen);
                return; // Don't forward the hotkey
            }

            if (mHasVirtualCursor) {
                InputEventData data;
                data.x = mVirtualCursor.x;
                data.y = mVirtualCursor.y;
                data.timestamp = event.timestamp;
                data.keyboardModifiers = event.state.keyboardModifiers;
                data.keycode = remapKeycode(event.keycode);
                data.text = event.text;

                broadcastInputEvent(event.type == EventType::KeyPress ? "keyPress" : "keyRelease", data);
            }
            break;
        }

        case EventType::MouseScroll: {
            if (mHasVirtualCursor) {
                InputEventData data;
                data.x = mVirtualCursor.x;
                data.y = mVirtualCursor.y;
                data.scrollX = event.state.scrollX;
                data.scrollY = event.state.scrollY;
                data.timestamp = event.timestamp;
                data.keyboardModifiers = event.state.keyboardModifiers;

                broadcastInputEvent("scroll", data);
            }
            break;
        }

        case EventType::DesktopChanged:
            // Handle desktop change if needed
            break;
    }
}

void Konflikt::onWebSocketMessage(const std::string &message, void *connection)
{
    auto msgType = getMessageType(message);
    if (!msgType) {
        log("error", "Failed to parse message type");
        return;
    }

    if (*msgType == "handshake_request") {
        auto req = fromJson<HandshakeRequest>(message);
        if (req)
            handleHandshakeRequest(*req, connection);
    } else if (*msgType == "handshake_response") {
        auto resp = fromJson<HandshakeResponse>(message);
        if (resp)
            handleHandshakeResponse(*resp);
    } else if (*msgType == "input_event") {
        auto ev = fromJson<InputEventMessage>(message);
        if (ev)
            handleInputEvent(*ev);
    } else if (*msgType == "client_registration") {
        auto reg = fromJson<ClientRegistrationMessage>(message);
        if (reg)
            handleClientRegistration(*reg);
    } else if (*msgType == "layout_assignment") {
        auto la = fromJson<LayoutAssignmentMessage>(message);
        if (la)
            handleLayoutAssignment(*la);
    } else if (*msgType == "layout_update") {
        auto lu = fromJson<LayoutUpdateMessage>(message);
        if (lu)
            handleLayoutUpdate(*lu);
    } else if (*msgType == "activate_client") {
        auto ac = fromJson<ActivateClientMessage>(message);
        if (ac)
            handleActivateClient(*ac);
    } else if (*msgType == "deactivation_request") {
        auto dr = fromJson<DeactivationRequestMessage>(message);
        if (dr)
            handleDeactivationRequest(*dr);
    } else if (*msgType == "clipboard_sync") {
        auto cs = fromJson<ClipboardSyncMessage>(message);
        if (cs)
            handleClipboardSync(*cs);
    } else if (*msgType == "server_shutdown") {
        auto ss = fromJson<ServerShutdownMessage>(message);
        if (ss)
            handleServerShutdown(*ss);
    }
}

void Konflikt::onClientConnected(void *connection)
{
    log("log", "Client connected");
    // Connection tracking happens after handshake
    (void)connection;
}

void Konflikt::onClientDisconnected(void *connection)
{
    auto it = mConnectionToInstanceId.find(connection);
    if (it != mConnectionToInstanceId.end()) {
        std::string instanceId = it->second;
        log("log", "Client disconnected: " + instanceId);

        // If this was the active client, deactivate remote screen
        if (instanceId == mActivatedClientId) {
            deactivateRemoteScreen();
        }

        if (mLayoutManager) {
            mLayoutManager->setClientOnline(instanceId, false);
        }
        mConnectionToInstanceId.erase(it);
        mConnectedClients.erase(instanceId);
    }
}

void Konflikt::handleHandshakeRequest(const HandshakeRequest &request, void *connection)
{
    log("log", "Handshake from " + request.instanceName);

    // Track connection
    mConnectionToInstanceId[connection] = request.instanceId;

    // Send response
    HandshakeResponse response;
    response.accepted = true;
    response.instanceId = mConfig.instanceId;
    response.instanceName = mConfig.instanceName;
    response.version = VERSION;
    response.capabilities = { "input_events", "screen_info" };
    response.timestamp = timestamp();

    mWsServer->send(connection, toJson(response));
}

void Konflikt::handleHandshakeResponse(const HandshakeResponse &response)
{
    if (response.accepted) {
        mConnectedServerName = response.instanceName;
        log("log", "Handshake completed with " + response.instanceName);

        // Send client registration
        ClientRegistrationMessage reg;
        reg.instanceId = mConfig.instanceId;
        reg.displayName = mConfig.instanceName;
        reg.machineId = mMachineId;
        reg.screenWidth = mScreenBounds.width;
        reg.screenHeight = mScreenBounds.height;

        mWsClient->send(toJson(reg));
    }
}

void Konflikt::handleInputEvent(const InputEventMessage &message)
{
    // Only clients execute received input events
    if (mConfig.role != InstanceRole::Client || !mIsActiveInstance) {
        return;
    }

    // Don't execute our own events
    if (message.sourceInstanceId == mConfig.instanceId) {
        return;
    }

    Event event;
    event.timestamp = message.eventData.timestamp;
    event.state.x = message.eventData.x;
    event.state.y = message.eventData.y;
    event.state.dx = message.eventData.dx;
    event.state.dy = message.eventData.dy;
    event.state.keyboardModifiers = message.eventData.keyboardModifiers;
    event.state.mouseButtons = message.eventData.mouseButtons;
    event.keycode = message.eventData.keycode;
    event.text = message.eventData.text;

    if (message.eventData.button == "left")
        event.button = MouseButton::Left;
    else if (message.eventData.button == "right")
        event.button = MouseButton::Right;
    else if (message.eventData.button == "middle")
        event.button = MouseButton::Middle;

    if (message.eventType == "mouseMove" || message.eventType == "mousePress" ||
        message.eventType == "mouseRelease") {
        mPlatform->sendMouseEvent(event);

        // Check for deactivation (cursor at left edge moving left)
        if (message.eventType == "mouseMove") {
            InputState state = mPlatform->getState();
            if (state.x <= 1 && message.eventData.dx < 0) {
                requestDeactivation();
            }
        }
    } else if (message.eventType == "scroll") {
        event.type = EventType::MouseScroll;
        event.state.scrollX = message.eventData.scrollX;
        event.state.scrollY = message.eventData.scrollY;
        mPlatform->sendMouseEvent(event);
    } else if (message.eventType == "keyPress" || message.eventType == "keyRelease") {
        event.type = message.eventType == "keyPress" ? EventType::KeyPress : EventType::KeyRelease;
        mPlatform->sendKeyEvent(event);
    }
}

void Konflikt::handleClientRegistration(const ClientRegistrationMessage &message)
{
    if (mConfig.role != InstanceRole::Server || !mLayoutManager) {
        return;
    }

    log("log", "Client registered: " + message.displayName);

    // Track client details
    ConnectedClient client;
    client.instanceId = message.instanceId;
    client.displayName = message.displayName;
    client.screenWidth = message.screenWidth;
    client.screenHeight = message.screenHeight;
    client.connectedAt = timestamp();
    client.active = false;
    mConnectedClients[message.instanceId] = client;

    auto entry = mLayoutManager->registerClient(
        message.instanceId,
        message.displayName,
        message.machineId,
        message.screenWidth,
        message.screenHeight);

    // Send layout assignment
    LayoutAssignmentMessage assignment;
    assignment.position.x = entry.x;
    assignment.position.y = entry.y;
    assignment.adjacency = mLayoutManager->getAdjacencyFor(message.instanceId);
    for (const auto &screen : mLayoutManager->getLayout()) {
        ScreenInfo info;
        info.instanceId = screen.instanceId;
        info.displayName = screen.displayName;
        info.x = screen.x;
        info.y = screen.y;
        info.width = screen.width;
        info.height = screen.height;
        info.isServer = screen.isServer;
        info.online = screen.online;
        assignment.fullLayout.push_back(info);
    }

    broadcastToClients(toJson(assignment));
}

void Konflikt::handleLayoutAssignment(const LayoutAssignmentMessage &message)
{
    if (mConfig.role != InstanceRole::Client) {
        return;
    }

    mScreenBounds.x = message.position.x;
    mScreenBounds.y = message.position.y;

    log("log", "Layout assigned: position (" + std::to_string(message.position.x) + ", " + std::to_string(message.position.y) + ")");
}

void Konflikt::handleLayoutUpdate(const LayoutUpdateMessage &message)
{
    if (mConfig.role != InstanceRole::Client) {
        return;
    }

    // Update our position from the layout
    for (const auto &screen : message.screens) {
        if (screen.instanceId == mConfig.instanceId) {
            mScreenBounds.x = screen.x;
            mScreenBounds.y = screen.y;
            break;
        }
    }
}

void Konflikt::handleActivateClient(const ActivateClientMessage &message)
{
    if (message.targetInstanceId != mConfig.instanceId) {
        // Not for us
        if (mIsActiveInstance) {
            mIsActiveInstance = false;
        }
        return;
    }

    log("log", "Activated at (" + std::to_string(message.cursorX) + ", " + std::to_string(message.cursorY) + ")");
    mIsActiveInstance = true;

    // Move cursor to specified position
    Event moveEvent;
    moveEvent.type = EventType::MouseMove;
    moveEvent.state.x = message.cursorX;
    moveEvent.state.y = message.cursorY;
    moveEvent.timestamp = timestamp();
    mPlatform->sendMouseEvent(moveEvent);
}

void Konflikt::handleDeactivationRequest(const DeactivationRequestMessage &message)
{
    if (mConfig.role != InstanceRole::Server) {
        return;
    }

    if (message.instanceId != mActivatedClientId) {
        return;
    }

    log("log", "Deactivation request from " + message.instanceId);
    deactivateRemoteScreen();
}

bool Konflikt::checkScreenTransition(int32_t x, int32_t y)
{
    if (mConfig.role != InstanceRole::Server || !mLayoutManager) {
        return false;
    }

    // Check if cursor is locked to screen
    if (mConfig.lockCursorToScreen) {
        return false;
    }

    // Cooldown after deactivation
    if (timestamp() - mLastDeactivationTime < 500) {
        return false;
    }

    const int32_t EDGE_THRESHOLD = 1;
    Side edge;
    bool atEdge = false;

    if (x <= mScreenBounds.x + EDGE_THRESHOLD && mConfig.edgeLeft) {
        edge = Side::Left;
        atEdge = true;
    } else if (x >= mScreenBounds.x + mScreenBounds.width - EDGE_THRESHOLD - 1 && mConfig.edgeRight) {
        edge = Side::Right;
        atEdge = true;
    } else if (y <= mScreenBounds.y + EDGE_THRESHOLD && mConfig.edgeTop) {
        edge = Side::Top;
        atEdge = true;
    } else if (y >= mScreenBounds.y + mScreenBounds.height - EDGE_THRESHOLD - 1 && mConfig.edgeBottom) {
        edge = Side::Bottom;
        atEdge = true;
    }

    if (!atEdge) {
        return false;
    }

    auto target = mLayoutManager->getTransitionTargetAtEdge(mConfig.instanceId, edge, x, y);
    if (!target) {
        return false;
    }

    // Only activate once
    if (mActivatedClientId == target->targetScreen.instanceId) {
        return true;
    }

    activateClient(target->targetScreen.instanceId, target->newX, target->newY);
    return true;
}

void Konflikt::activateClient(const std::string &targetInstanceId, int32_t cursorX, int32_t cursorY)
{
    // Clear active flag on previous client
    if (!mActivatedClientId.empty()) {
        auto prevIt = mConnectedClients.find(mActivatedClientId);
        if (prevIt != mConnectedClients.end()) {
            prevIt->second.active = false;
        }
    }

    mActivatedClientId = targetInstanceId;

    // Set active flag on new client
    auto it = mConnectedClients.find(targetInstanceId);
    if (it != mConnectedClients.end()) {
        it->second.active = true;
    }

    ActivateClientMessage msg;
    msg.targetInstanceId = targetInstanceId;
    msg.cursorX = cursorX;
    msg.cursorY = cursorY;
    msg.timestamp = timestamp();

    broadcastToClients(toJson(msg));

    // Set up virtual cursor
    mVirtualCursor.x = cursorX;
    mVirtualCursor.y = cursorY;
    mHasVirtualCursor = true;

    auto screen = mLayoutManager->getScreen(targetInstanceId);
    if (screen) {
        mActiveRemoteScreenBounds = Rect(0, 0, screen->width, screen->height);
    }

    // Hide cursor on server
    mPlatform->hideCursor();
    mIsActiveInstance = false;

    log("log", "Activated client " + targetInstanceId);
}

void Konflikt::deactivateRemoteScreen()
{
    // Clear active flag on deactivated client
    if (!mActivatedClientId.empty()) {
        auto it = mConnectedClients.find(mActivatedClientId);
        if (it != mConnectedClients.end()) {
            it->second.active = false;
        }
    }

    mVirtualCursor = { 0, 0 };
    mHasVirtualCursor = false;
    mActivatedClientId.clear();
    mActiveRemoteScreenBounds = Rect();

    // Show cursor
    mPlatform->showCursor();

    // Warp cursor to right edge
    int32_t rightEdgeX = mScreenBounds.x + mScreenBounds.width - 1;
    InputState state = mPlatform->getState();

    Event moveEvent;
    moveEvent.type = EventType::MouseMove;
    moveEvent.state.x = rightEdgeX;
    moveEvent.state.y = state.y;
    moveEvent.timestamp = timestamp();
    mPlatform->sendMouseEvent(moveEvent);

    mIsActiveInstance = true;
    mLastDeactivationTime = timestamp();

    log("log", "Deactivated remote screen");
}

void Konflikt::requestDeactivation()
{
    if (timestamp() - mLastDeactivationRequest < 500) {
        return;
    }
    mLastDeactivationRequest = timestamp();

    DeactivationRequestMessage msg;
    msg.instanceId = mConfig.instanceId;
    msg.timestamp = timestamp();

    mWsClient->send(toJson(msg));
    log("log", "Requested deactivation");
}

void Konflikt::broadcastInputEvent(const std::string &eventType, const InputEventData &data)
{
    InputEventMessage msg;
    msg.sourceInstanceId = mConfig.instanceId;
    msg.sourceDisplayId = mDisplayId;
    msg.sourceMachineId = mMachineId;
    msg.eventType = eventType;
    msg.eventData = data;

    broadcastToClients(toJson(msg));
}

void Konflikt::broadcastToClients(const std::string &message)
{
    if (mWsServer) {
        mWsServer->broadcast(message);
    }
}

void Konflikt::updateStatus(ConnectionStatus status, const std::string &message)
{
    mConnectionStatus = status;
    if (mStatusCallback) {
        mStatusCallback(status, message);
    }
}

void Konflikt::log(const std::string &level, const std::string &message)
{
    if (mLogCallback) {
        mLogCallback(level, message);
    }

    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    struct tm tm_buf;
    localtime_r(&time, &tm_buf);

    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d.%03d",
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
             static_cast<int>(ms.count()));

    // Print to stderr for debugging
    if (mConfig.verbose || level == "error" || level == "log") {
        fprintf(stderr, "[%s] [%s] %s\n", timeStr, level.c_str(), message.c_str());
    }

    // Store in log buffer for debug API (filter sensitive key data)
    if (mConfig.enableDebugApi) {
        std::string filteredMessage = message;

        // Filter out keycode and key text from messages
        // Patterns like "keycode=123" or "key='x'" are sensitive
        auto filterPattern = [&filteredMessage](const std::string &pattern, const std::string &replacement) {
            size_t pos = 0;
            while ((pos = filteredMessage.find(pattern, pos)) != std::string::npos) {
                size_t endPos = pos + pattern.length();
                // Find the end of the value
                while (endPos < filteredMessage.length() &&
                       filteredMessage[endPos] != ' ' &&
                       filteredMessage[endPos] != ',' &&
                       filteredMessage[endPos] != ')') {
                    endPos++;
                }
                filteredMessage.replace(pos, endPos - pos, replacement);
                pos += replacement.length();
            }
        };

        filterPattern("keycode=", "keycode=[redacted]");
        filterPattern("text=", "text=[redacted]");
        filterPattern("key=", "key=[redacted]");

        std::lock_guard<std::mutex> lock(mLogBufferMutex);
        mLogBuffer.push_back({ timeStr, level, filteredMessage });

        // Keep buffer bounded
        if (mLogBuffer.size() > MAX_LOG_ENTRIES) {
            mLogBuffer.erase(mLogBuffer.begin());
        }
    }
}

std::string Konflikt::generateMachineId()
{
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    std::string input = std::string(hostname) + "-" + std::to_string(getuid());

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.length(), hash);

    std::stringstream ss;
    for (int i = 0; i < 8; i++) {
        ss << std::hex << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string Konflikt::generateDisplayId()
{
    Desktop desktop = mPlatform->getDesktop();
    std::string input = mMachineId + "-" +
        std::to_string(desktop.width) + "x" + std::to_string(desktop.height) + "-" +
        std::to_string(mScreenBounds.x) + "," + std::to_string(mScreenBounds.y);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.length(), hash);

    std::stringstream ss;
    for (int i = 0; i < 8; i++) {
        ss << std::hex << static_cast<int>(hash[i]);
    }
    return ss.str();
}

void Konflikt::handleClipboardSync(const ClipboardSyncMessage &message)
{
    // Don't apply our own clipboard updates
    if (message.sourceInstanceId == mConfig.instanceId) {
        return;
    }

    // Only accept newer clipboard data
    if (message.sequence <= mClipboardSequence) {
        return;
    }

    mClipboardSequence = message.sequence;

    // Currently only supporting plain text
    if (message.format == "text/plain") {
        mLastClipboardText = message.data;
        if (mPlatform) {
            mPlatform->setClipboardText(message.data);
        }
        if (mConfig.verbose) {
            log("verbose", "Clipboard synced from " + message.sourceInstanceId);
        }
    }
}

void Konflikt::handleServerShutdown(const ServerShutdownMessage &message)
{
    log("log", "Server shutting down: " + message.reason);

    // Mark that we're expecting reconnection (graceful shutdown)
    mExpectingReconnect = true;
    mExpectedRestartDelayMs = message.delayMs;

    // Reset reconnection attempts since this is a graceful shutdown
    mReconnectAttempts = 0;

    // Update status to inform user
    updateStatus(ConnectionStatus::Disconnected, "Server shutdown: " + message.reason);
}

void Konflikt::notifyShutdown(const std::string &reason, int32_t delayMs)
{
    if (mConfig.role != InstanceRole::Server) {
        return;
    }

    ServerShutdownMessage message;
    message.reason = reason;
    message.delayMs = delayMs;
    message.timestamp = timestamp();

    broadcastToClients(toJson(message));
    log("log", "Sent shutdown notification to clients: " + reason);
}

void Konflikt::setLockCursorToScreen(bool locked)
{
    mConfig.lockCursorToScreen = locked;
    log("log", locked ? "Cursor locked to screen" : "Cursor unlocked");
}

void Konflikt::setEdgeTransitions(bool left, bool right, bool top, bool bottom)
{
    mConfig.edgeLeft = left;
    mConfig.edgeRight = right;
    mConfig.edgeTop = top;
    mConfig.edgeBottom = bottom;
    log("log", std::string("Edge transitions: ") +
        "L=" + (left ? "on" : "off") + " " +
        "R=" + (right ? "on" : "off") + " " +
        "T=" + (top ? "on" : "off") + " " +
        "B=" + (bottom ? "on" : "off"));
}

bool Konflikt::saveConfig(const std::string &path)
{
    bool success = ConfigManager::save(mConfig, path);
    if (success) {
        log("log", "Configuration saved");
    } else {
        log("error", "Failed to save configuration");
    }
    return success;
}

void Konflikt::checkClipboardChange()
{
    if (!mPlatform) {
        return;
    }

    // Poll clipboard periodically (every 500ms)
    uint64_t now = timestamp();
    if (now - mLastClipboardCheck < 500) {
        return;
    }
    mLastClipboardCheck = now;

    std::string currentText = mPlatform->getClipboardText();

    // Check if clipboard changed
    if (!currentText.empty() && currentText != mLastClipboardText) {
        mLastClipboardText = currentText;
        broadcastClipboard(currentText);
    }
}

void Konflikt::broadcastClipboard(const std::string &text)
{
    mClipboardSequence++;

    ClipboardSyncMessage msg;
    msg.sourceInstanceId = mConfig.instanceId;
    msg.format = "text/plain";
    msg.data = text;
    msg.sequence = mClipboardSequence;
    msg.timestamp = timestamp();

    std::string json = toJson(msg);

    // Server broadcasts to all clients
    if (mConfig.role == InstanceRole::Server) {
        broadcastToClients(json);
    } else if (mWsClient) {
        // Client sends to server (which will relay)
        mWsClient->send(json);
    }

    if (mConfig.verbose) {
        log("verbose", "Broadcasting clipboard change");
    }
}

void Konflikt::onServiceFound(const DiscoveredService &service)
{
    log("log", "Discovered server: " + service.name + " at " + service.host + ":" + std::to_string(service.port));

    // Don't connect to ourselves
    if (service.instanceId == mConfig.instanceId) {
        return;
    }

    // Auto-connect if we're a client without a connection
    if (mConfig.role == InstanceRole::Client &&
        mConnectionStatus != ConnectionStatus::Connected &&
        mConfig.serverHost.empty()) {
        connectToDiscoveredServer(service.host, service.port);
    }
}

void Konflikt::onServiceLost(const std::string &name)
{
    log("log", "Server disappeared: " + name);
}

void Konflikt::connectToDiscoveredServer(const std::string &host, int port)
{
    if (!mWsClient) {
        return;
    }

    // Don't reconnect if already connected
    if (mConnectionStatus == ConnectionStatus::Connected) {
        return;
    }

    log("log", "Auto-connecting to discovered server: " + host + ":" + std::to_string(port));
    updateStatus(ConnectionStatus::Connecting, "Connecting to " + host + "...");
    mWsClient->connect(host, port, "/ws");
}

} // namespace konflikt
