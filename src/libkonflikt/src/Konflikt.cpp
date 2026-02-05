#include "konflikt/Konflikt.h"
#include "konflikt/HttpServer.h"
#include "konflikt/LayoutManager.h"
#include "konflikt/WebSocketClient.h"
#include "konflikt/WebSocketServer.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <openssl/sha.h>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace konflikt {

Konflikt::Konflikt(const Config &config)
    : m_config(config)
{
    // Generate identifiers
    m_machineId = generateMachineId();
}

Konflikt::~Konflikt()
{
    stop();
}

bool Konflikt::init()
{
    // Set up logger
    m_logger.verbose = [this](const std::string &msg) {
        log("verbose", msg);
    };
    m_logger.debug = [this](const std::string &msg) {
        log("debug", msg);
    };
    m_logger.log = [this](const std::string &msg) {
        log("log", msg);
    };
    m_logger.error = [this](const std::string &msg) {
        log("error", msg);
    };

    // Create platform
    m_platform = createPlatform();
    if (!m_platform || !m_platform->initialize(m_logger)) {
        log("error", "Failed to initialize platform");
        return false;
    }

    // Get screen bounds
    Desktop desktop = m_platform->getDesktop();
    m_screenBounds = Rect(
        m_config.screenX,
        m_config.screenY,
        m_config.screenWidth > 0 ? m_config.screenWidth : desktop.width,
        m_config.screenHeight > 0 ? m_config.screenHeight : desktop.height);

    m_displayId = generateDisplayId();

    log("log", "Screen bounds: " + std::to_string(m_screenBounds.width) + "x" + std::to_string(m_screenBounds.height));

    // Create WebSocket server
    m_wsServer = std::make_unique<WebSocketServer>(m_config.port);
    m_wsServer->setCallbacks({ .onConnect = [this](void *conn) {
        onClientConnected(conn);
    }, .onDisconnect = [this](void *conn) {
        onClientDisconnected(conn);
    }, .onMessage = [this](const std::string &msg, void *conn) {
        onWebSocketMessage(msg, conn);
    } });

    // Create HTTP server (same port as WebSocket for now)
    m_httpServer = std::make_unique<HttpServer>(m_config.port);

    // Serve static UI files if path is configured
    if (!m_config.uiPath.empty() && std::filesystem::exists(m_config.uiPath)) {
        m_httpServer->serveStatic("/ui/", m_config.uiPath);
        log("log", "Serving UI from " + m_config.uiPath);
    }

    // Set up platform event handler for server role
    if (m_config.role == InstanceRole::Server) {
        m_layoutManager = std::make_unique<LayoutManager>();
        m_layoutManager->setServerScreen(
            m_config.instanceId,
            m_config.instanceName,
            m_machineId,
            m_screenBounds.width,
            m_screenBounds.height);

        m_platform->onEvent = [this](const Event &event) {
            onPlatformEvent(event);
        };

        m_platform->startListening();
        m_isActiveInstance = true;
    } else {
        // Client role: create WebSocket client
        m_wsClient = std::make_unique<WebSocketClient>();
        m_wsClient->setCallbacks({ .onConnect = [this]() {
            updateStatus(ConnectionStatus::Connected, "Connected to server");
                // Send handshake
            HandshakeRequest req;
            req.instanceId = m_config.instanceId;
            req.instanceName = m_config.instanceName;
            req.version = "2.0.0";
            req.capabilities = { "input_events", "screen_info" };
            req.timestamp = timestamp();
            m_wsClient->send(toJson(req));
        }, .onDisconnect = [this](const std::string &reason) {
            updateStatus(ConnectionStatus::Disconnected, reason);
        }, .onMessage = [this](const std::string &msg) {
            onWebSocketMessage(msg, nullptr);
        }, .onError = [this](const std::string &err) {
            updateStatus(ConnectionStatus::Error, err);
        } });
    }

    return true;
}

void Konflikt::run()
{
    m_running = true;

    // Start servers
    if (m_config.role == InstanceRole::Server) {
        if (!m_wsServer->start()) {
            log("error", "Failed to start WebSocket server");
            return;
        }
        if (!m_httpServer->start()) {
            log("error", "Failed to start HTTP server");
            return;
        }
        log("log", "Server listening on port " + std::to_string(m_wsServer->port()));
        updateStatus(ConnectionStatus::Connected, "Server running");
    } else {
        // Client: connect to server
        if (!m_config.serverHost.empty()) {
            log("log", "Connecting to " + m_config.serverHost + ":" + std::to_string(m_config.serverPort));
            updateStatus(ConnectionStatus::Connecting, "Connecting...");
            m_wsClient->connect(m_config.serverHost, m_config.serverPort, "/ws");
        }
    }

    // Main loop
    while (m_running) {
        if (m_wsClient) {
            m_wsClient->poll();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Konflikt::stop()
{
    m_running = false;

    if (m_platform) {
        m_platform->stopListening();
        m_platform->shutdown();
    }

    if (m_wsServer) {
        m_wsServer->stop();
    }

    if (m_httpServer) {
        m_httpServer->stop();
    }

    if (m_wsClient) {
        m_wsClient->disconnect();
    }
}

void Konflikt::quit()
{
    m_running = false;
}

int Konflikt::httpPort() const
{
    return m_httpServer ? m_httpServer->port() : m_config.port;
}

void Konflikt::onPlatformEvent(const Event &event)
{
    switch (event.type) {
        case EventType::MouseMove: {
            // Update local cursor position
            if (m_hasVirtualCursor && m_activatedClientId.empty() == false) {
                // Update virtual cursor
                int32_t newX = m_virtualCursor.x + event.state.dx;
                int32_t newY = m_virtualCursor.y + event.state.dy;

                m_virtualCursor.x = std::clamp(newX, 0, m_activeRemoteScreenBounds.width - 1);
                m_virtualCursor.y = std::clamp(newY, 0, m_activeRemoteScreenBounds.height - 1);

                InputEventData data;
                data.x = m_virtualCursor.x;
                data.y = m_virtualCursor.y;
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
            if (m_hasVirtualCursor) {
                InputEventData data;
                data.x = m_virtualCursor.x;
                data.y = m_virtualCursor.y;
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
            if (m_hasVirtualCursor) {
                InputEventData data;
                data.x = m_virtualCursor.x;
                data.y = m_virtualCursor.y;
                data.timestamp = event.timestamp;
                data.keyboardModifiers = event.state.keyboardModifiers;
                data.keycode = event.keycode;
                data.text = event.text;

                broadcastInputEvent(event.type == EventType::KeyPress ? "keyPress" : "keyRelease", data);
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
    auto it = m_connectionToInstanceId.find(connection);
    if (it != m_connectionToInstanceId.end()) {
        std::string instanceId = it->second;
        log("log", "Client disconnected: " + instanceId);

        // If this was the active client, deactivate remote screen
        if (instanceId == m_activatedClientId) {
            deactivateRemoteScreen();
        }

        if (m_layoutManager) {
            m_layoutManager->setClientOnline(instanceId, false);
        }
        m_connectionToInstanceId.erase(it);
    }
}

void Konflikt::handleHandshakeRequest(const HandshakeRequest &request, void *connection)
{
    log("log", "Handshake from " + request.instanceName);

    // Track connection
    m_connectionToInstanceId[connection] = request.instanceId;

    // Send response
    HandshakeResponse response;
    response.accepted = true;
    response.instanceId = m_config.instanceId;
    response.instanceName = m_config.instanceName;
    response.version = "2.0.0";
    response.capabilities = { "input_events", "screen_info" };
    response.timestamp = timestamp();

    m_wsServer->send(connection, toJson(response));
}

void Konflikt::handleHandshakeResponse(const HandshakeResponse &response)
{
    if (response.accepted) {
        m_connectedServerName = response.instanceName;
        log("log", "Handshake completed with " + response.instanceName);

        // Send client registration
        ClientRegistrationMessage reg;
        reg.instanceId = m_config.instanceId;
        reg.displayName = m_config.instanceName;
        reg.machineId = m_machineId;
        reg.screenWidth = m_screenBounds.width;
        reg.screenHeight = m_screenBounds.height;

        m_wsClient->send(toJson(reg));
    }
}

void Konflikt::handleInputEvent(const InputEventMessage &message)
{
    // Only clients execute received input events
    if (m_config.role != InstanceRole::Client || !m_isActiveInstance) {
        return;
    }

    // Don't execute our own events
    if (message.sourceInstanceId == m_config.instanceId) {
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
        m_platform->sendMouseEvent(event);

        // Check for deactivation (cursor at left edge moving left)
        if (message.eventType == "mouseMove") {
            InputState state = m_platform->getState();
            if (state.x <= 1 && message.eventData.dx < 0) {
                requestDeactivation();
            }
        }
    } else if (message.eventType == "keyPress" || message.eventType == "keyRelease") {
        event.type = message.eventType == "keyPress" ? EventType::KeyPress : EventType::KeyRelease;
        m_platform->sendKeyEvent(event);
    }
}

void Konflikt::handleClientRegistration(const ClientRegistrationMessage &message)
{
    if (m_config.role != InstanceRole::Server || !m_layoutManager) {
        return;
    }

    log("log", "Client registered: " + message.displayName);

    auto entry = m_layoutManager->registerClient(
        message.instanceId,
        message.displayName,
        message.machineId,
        message.screenWidth,
        message.screenHeight);

    // Send layout assignment
    LayoutAssignmentMessage assignment;
    assignment.position.x = entry.x;
    assignment.position.y = entry.y;
    assignment.adjacency = m_layoutManager->getAdjacencyFor(message.instanceId);
    for (const auto &screen : m_layoutManager->getLayout()) {
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
    if (m_config.role != InstanceRole::Client) {
        return;
    }

    m_screenBounds.x = message.position.x;
    m_screenBounds.y = message.position.y;

    log("log", "Layout assigned: position (" + std::to_string(message.position.x) + ", " + std::to_string(message.position.y) + ")");
}

void Konflikt::handleLayoutUpdate(const LayoutUpdateMessage &message)
{
    if (m_config.role != InstanceRole::Client) {
        return;
    }

    // Update our position from the layout
    for (const auto &screen : message.screens) {
        if (screen.instanceId == m_config.instanceId) {
            m_screenBounds.x = screen.x;
            m_screenBounds.y = screen.y;
            break;
        }
    }
}

void Konflikt::handleActivateClient(const ActivateClientMessage &message)
{
    if (message.targetInstanceId != m_config.instanceId) {
        // Not for us
        if (m_isActiveInstance) {
            m_isActiveInstance = false;
        }
        return;
    }

    log("log", "Activated at (" + std::to_string(message.cursorX) + ", " + std::to_string(message.cursorY) + ")");
    m_isActiveInstance = true;

    // Move cursor to specified position
    Event moveEvent;
    moveEvent.type = EventType::MouseMove;
    moveEvent.state.x = message.cursorX;
    moveEvent.state.y = message.cursorY;
    moveEvent.timestamp = timestamp();
    m_platform->sendMouseEvent(moveEvent);
}

void Konflikt::handleDeactivationRequest(const DeactivationRequestMessage &message)
{
    if (m_config.role != InstanceRole::Server) {
        return;
    }

    if (message.instanceId != m_activatedClientId) {
        return;
    }

    log("log", "Deactivation request from " + message.instanceId);
    deactivateRemoteScreen();
}

bool Konflikt::checkScreenTransition(int32_t x, int32_t y)
{
    if (m_config.role != InstanceRole::Server || !m_layoutManager) {
        return false;
    }

    // Cooldown after deactivation
    if (timestamp() - m_lastDeactivationTime < 500) {
        return false;
    }

    const int32_t EDGE_THRESHOLD = 1;
    Side edge;
    bool atEdge = false;

    if (x <= m_screenBounds.x + EDGE_THRESHOLD) {
        edge = Side::Left;
        atEdge = true;
    } else if (x >= m_screenBounds.x + m_screenBounds.width - EDGE_THRESHOLD - 1) {
        edge = Side::Right;
        atEdge = true;
    } else if (y <= m_screenBounds.y + EDGE_THRESHOLD) {
        edge = Side::Top;
        atEdge = true;
    } else if (y >= m_screenBounds.y + m_screenBounds.height - EDGE_THRESHOLD - 1) {
        edge = Side::Bottom;
        atEdge = true;
    }

    if (!atEdge) {
        return false;
    }

    auto target = m_layoutManager->getTransitionTargetAtEdge(m_config.instanceId, edge, x, y);
    if (!target) {
        return false;
    }

    // Only activate once
    if (m_activatedClientId == target->targetScreen.instanceId) {
        return true;
    }

    activateClient(target->targetScreen.instanceId, target->newX, target->newY);
    return true;
}

void Konflikt::activateClient(const std::string &targetInstanceId, int32_t cursorX, int32_t cursorY)
{
    m_activatedClientId = targetInstanceId;

    ActivateClientMessage msg;
    msg.targetInstanceId = targetInstanceId;
    msg.cursorX = cursorX;
    msg.cursorY = cursorY;
    msg.timestamp = timestamp();

    broadcastToClients(toJson(msg));

    // Set up virtual cursor
    m_virtualCursor.x = cursorX;
    m_virtualCursor.y = cursorY;
    m_hasVirtualCursor = true;

    auto screen = m_layoutManager->getScreen(targetInstanceId);
    if (screen) {
        m_activeRemoteScreenBounds = Rect(0, 0, screen->width, screen->height);
    }

    // Hide cursor on server
    m_platform->hideCursor();
    m_isActiveInstance = false;

    log("log", "Activated client " + targetInstanceId);
}

void Konflikt::deactivateRemoteScreen()
{
    m_virtualCursor = { 0, 0 };
    m_hasVirtualCursor = false;
    m_activatedClientId.clear();
    m_activeRemoteScreenBounds = Rect();

    // Show cursor
    m_platform->showCursor();

    // Warp cursor to right edge
    int32_t rightEdgeX = m_screenBounds.x + m_screenBounds.width - 1;
    InputState state = m_platform->getState();

    Event moveEvent;
    moveEvent.type = EventType::MouseMove;
    moveEvent.state.x = rightEdgeX;
    moveEvent.state.y = state.y;
    moveEvent.timestamp = timestamp();
    m_platform->sendMouseEvent(moveEvent);

    m_isActiveInstance = true;
    m_lastDeactivationTime = timestamp();

    log("log", "Deactivated remote screen");
}

void Konflikt::requestDeactivation()
{
    if (timestamp() - m_lastDeactivationRequest < 500) {
        return;
    }
    m_lastDeactivationRequest = timestamp();

    DeactivationRequestMessage msg;
    msg.instanceId = m_config.instanceId;
    msg.timestamp = timestamp();

    m_wsClient->send(toJson(msg));
    log("log", "Requested deactivation");
}

void Konflikt::broadcastInputEvent(const std::string &eventType, const InputEventData &data)
{
    InputEventMessage msg;
    msg.sourceInstanceId = m_config.instanceId;
    msg.sourceDisplayId = m_displayId;
    msg.sourceMachineId = m_machineId;
    msg.eventType = eventType;
    msg.eventData = data;

    broadcastToClients(toJson(msg));
}

void Konflikt::broadcastToClients(const std::string &message)
{
    if (m_wsServer) {
        m_wsServer->broadcast(message);
    }
}

void Konflikt::updateStatus(ConnectionStatus status, const std::string &message)
{
    m_connectionStatus = status;
    if (m_statusCallback) {
        m_statusCallback(status, message);
    }
}

void Konflikt::log(const std::string &level, const std::string &message)
{
    if (m_logCallback) {
        m_logCallback(level, message);
    }

    // Also print to stderr for debugging
    if (m_config.verbose || level == "error" || level == "log") {
        fprintf(stderr, "[%s] %s\n", level.c_str(), message.c_str());
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
    Desktop desktop = m_platform->getDesktop();
    std::string input = m_machineId + "-" +
        std::to_string(desktop.width) + "x" + std::to_string(desktop.height) + "-" +
        std::to_string(m_screenBounds.x) + "," + std::to_string(m_screenBounds.y);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.length(), hash);

    std::stringstream ss;
    for (int i = 0; i < 8; i++) {
        ss << std::hex << static_cast<int>(hash[i]);
    }
    return ss.str();
}

} // namespace konflikt
