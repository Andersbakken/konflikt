#pragma once

#include <cstdint>
#include <glaze/glaze.hpp>
#include <optional>
#include <string>
#include <vector>

namespace konflikt {

// ============================================================================
// Input Event Data
// ============================================================================

struct InputEventData
{
    int32_t x {};
    int32_t y {};
    int32_t dx {};
    int32_t dy {};
    uint64_t timestamp {};
    uint32_t keyboardModifiers {};
    uint32_t mouseButtons {};
    uint32_t keycode {};
    std::string button;
    std::string text;
};

// ============================================================================
// Screen/Layout Types
// ============================================================================

struct ScreenInfo
{
    std::string instanceId;
    std::string displayName;
    int32_t x {};
    int32_t y {};
    int32_t width {};
    int32_t height {};
    bool isServer { false };
    bool online { true };
};

struct Position
{
    int32_t x {};
    int32_t y {};
};

struct Adjacency
{
    std::optional<std::string> left;
    std::optional<std::string> right;
    std::optional<std::string> top;
    std::optional<std::string> bottom;
};

struct ScreenGeometry
{
    int32_t x {};
    int32_t y {};
    int32_t width {};
    int32_t height {};
};

// ============================================================================
// Protocol Messages
// ============================================================================

/// Base fields present in most messages
struct BaseMessage
{
    std::string type;
    uint64_t timestamp {};
};

/// Handshake request from client to server
struct HandshakeRequest
{
    std::string type = "handshake_request";
    std::string instanceId;
    std::string instanceName;
    std::string version;
    std::vector<std::string> capabilities;
    std::optional<std::string> gitCommit;
    uint64_t timestamp {};
};

/// Handshake response from server to client
struct HandshakeResponse
{
    std::string type = "handshake_response";
    bool accepted { true };
    std::string instanceId;
    std::string instanceName;
    std::string version;
    std::vector<std::string> capabilities;
    std::optional<std::string> gitCommit;
    uint64_t timestamp {};
};

/// Input event message (mouse/keyboard)
struct InputEventMessage
{
    std::string type = "input_event";
    std::string sourceInstanceId;
    std::string sourceDisplayId;
    std::string sourceMachineId;
    std::string eventType; // mouseMove, mousePress, mouseRelease, keyPress, keyRelease
    InputEventData eventData;
};

/// Client registration message
struct ClientRegistrationMessage
{
    std::string type = "client_registration";
    std::string instanceId;
    std::string displayName;
    std::string machineId;
    int32_t screenWidth {};
    int32_t screenHeight {};
};

/// Instance info message
struct InstanceInfoMessage
{
    std::string type = "instance_info";
    std::string instanceId;
    std::string displayId;
    std::string machineId;
    uint64_t timestamp {};
    ScreenGeometry screenGeometry;
};

/// Layout assignment from server to client
struct LayoutAssignmentMessage
{
    std::string type = "layout_assignment";
    Position position;
    Adjacency adjacency;
    std::vector<ScreenInfo> fullLayout;
};

/// Layout update broadcast
struct LayoutUpdateMessage
{
    std::string type = "layout_update";
    std::vector<ScreenInfo> screens;
    uint64_t timestamp {};
};

/// Activate client message
struct ActivateClientMessage
{
    std::string type = "activate_client";
    std::string targetInstanceId;
    int32_t cursorX {};
    int32_t cursorY {};
    uint64_t timestamp {};
};

/// Deactivation request from client
struct DeactivationRequestMessage
{
    std::string type = "deactivation_request";
    std::string instanceId;
    uint64_t timestamp {};
};

/// Heartbeat message
struct HeartbeatMessage
{
    std::string type = "heartbeat";
    uint64_t timestamp {};
};

/// Update required message
struct UpdateRequiredMessage
{
    std::string type = "update_required";
    std::string serverCommit;
    std::string clientCommit;
    uint64_t timestamp {};
};

// ============================================================================
// Glaze Metadata (for JSON serialization)
// ============================================================================

} // namespace konflikt

// Glaze template specializations for automatic JSON serialization
template <>
struct glz::meta<konflikt::InputEventData>
{
    using T = konflikt::InputEventData;
    static constexpr auto value = object(
        "x", &T::x,
        "y", &T::y,
        "dx", &T::dx,
        "dy", &T::dy,
        "timestamp", &T::timestamp,
        "keyboardModifiers", &T::keyboardModifiers,
        "mouseButtons", &T::mouseButtons,
        "keycode", &T::keycode,
        "button", &T::button,
        "text", &T::text);
};

template <>
struct glz::meta<konflikt::ScreenInfo>
{
    using T = konflikt::ScreenInfo;
    static constexpr auto value = object(
        "instanceId", &T::instanceId,
        "displayName", &T::displayName,
        "x", &T::x,
        "y", &T::y,
        "width", &T::width,
        "height", &T::height,
        "isServer", &T::isServer,
        "online", &T::online);
};

template <>
struct glz::meta<konflikt::Position>
{
    using T = konflikt::Position;
    static constexpr auto value = object("x", &T::x, "y", &T::y);
};

template <>
struct glz::meta<konflikt::Adjacency>
{
    using T = konflikt::Adjacency;
    static constexpr auto value = object(
        "left", &T::left,
        "right", &T::right,
        "top", &T::top,
        "bottom", &T::bottom);
};

template <>
struct glz::meta<konflikt::ScreenGeometry>
{
    using T = konflikt::ScreenGeometry;
    static constexpr auto value = object(
        "x", &T::x,
        "y", &T::y,
        "width", &T::width,
        "height", &T::height);
};

template <>
struct glz::meta<konflikt::HandshakeRequest>
{
    using T = konflikt::HandshakeRequest;
    static constexpr auto value = object(
        "type", &T::type,
        "instanceId", &T::instanceId,
        "instanceName", &T::instanceName,
        "version", &T::version,
        "capabilities", &T::capabilities,
        "gitCommit", &T::gitCommit,
        "timestamp", &T::timestamp);
};

template <>
struct glz::meta<konflikt::HandshakeResponse>
{
    using T = konflikt::HandshakeResponse;
    static constexpr auto value = object(
        "type", &T::type,
        "accepted", &T::accepted,
        "instanceId", &T::instanceId,
        "instanceName", &T::instanceName,
        "version", &T::version,
        "capabilities", &T::capabilities,
        "gitCommit", &T::gitCommit,
        "timestamp", &T::timestamp);
};

template <>
struct glz::meta<konflikt::InputEventMessage>
{
    using T = konflikt::InputEventMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "sourceInstanceId", &T::sourceInstanceId,
        "sourceDisplayId", &T::sourceDisplayId,
        "sourceMachineId", &T::sourceMachineId,
        "eventType", &T::eventType,
        "eventData", &T::eventData);
};

template <>
struct glz::meta<konflikt::ClientRegistrationMessage>
{
    using T = konflikt::ClientRegistrationMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "instanceId", &T::instanceId,
        "displayName", &T::displayName,
        "machineId", &T::machineId,
        "screenWidth", &T::screenWidth,
        "screenHeight", &T::screenHeight);
};

template <>
struct glz::meta<konflikt::InstanceInfoMessage>
{
    using T = konflikt::InstanceInfoMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "instanceId", &T::instanceId,
        "displayId", &T::displayId,
        "machineId", &T::machineId,
        "timestamp", &T::timestamp,
        "screenGeometry", &T::screenGeometry);
};

template <>
struct glz::meta<konflikt::LayoutAssignmentMessage>
{
    using T = konflikt::LayoutAssignmentMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "position", &T::position,
        "adjacency", &T::adjacency,
        "fullLayout", &T::fullLayout);
};

template <>
struct glz::meta<konflikt::LayoutUpdateMessage>
{
    using T = konflikt::LayoutUpdateMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "screens", &T::screens,
        "timestamp", &T::timestamp);
};

template <>
struct glz::meta<konflikt::ActivateClientMessage>
{
    using T = konflikt::ActivateClientMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "targetInstanceId", &T::targetInstanceId,
        "cursorX", &T::cursorX,
        "cursorY", &T::cursorY,
        "timestamp", &T::timestamp);
};

template <>
struct glz::meta<konflikt::DeactivationRequestMessage>
{
    using T = konflikt::DeactivationRequestMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "instanceId", &T::instanceId,
        "timestamp", &T::timestamp);
};

template <>
struct glz::meta<konflikt::HeartbeatMessage>
{
    using T = konflikt::HeartbeatMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "timestamp", &T::timestamp);
};

template <>
struct glz::meta<konflikt::UpdateRequiredMessage>
{
    using T = konflikt::UpdateRequiredMessage;
    static constexpr auto value = object(
        "type", &T::type,
        "serverCommit", &T::serverCommit,
        "clientCommit", &T::clientCommit,
        "timestamp", &T::timestamp);
};

namespace konflikt {

// ============================================================================
// Protocol Helper Functions
// ============================================================================

/// Parse message type from JSON without fully parsing
std::optional<std::string> getMessageType(std::string_view json);

/// Serialize any message to JSON
template <typename T>
std::string toJson(const T &message)
{
    return glz::write_json(message).value_or("");
}

/// Parse JSON to a specific message type
template <typename T>
std::optional<T> fromJson(std::string_view json)
{
    T result;
    auto error = glz::read_json(result, json);
    if (error) {
        return std::nullopt;
    }
    return result;
}

} // namespace konflikt
