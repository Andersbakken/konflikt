#pragma once

#include "Protocol.h"
#include "Rect.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace konflikt {

/// Screen entry in the layout
struct ScreenEntry
{
    std::string instanceId;
    std::string displayName;
    std::string machineId;
    int32_t x {};
    int32_t y {};
    int32_t width {};
    int32_t height {};
    bool isServer { false };
    bool online { true };
};

/// Transition target information
struct TransitionTarget
{
    ScreenEntry targetScreen;
    int32_t newX {};
    int32_t newY {};
};

/// Side of screen
enum class Side
{
    Left,
    Right,
    Top,
    Bottom
};

/// Layout manager for managing screen arrangements
class LayoutManager
{
public:
    LayoutManager();
    ~LayoutManager();

    /// Set the server's own screen
    void setServerScreen(const std::string &instanceId,
                         const std::string &displayName,
                         const std::string &machineId,
                         int32_t width, int32_t height);

    /// Register a client screen
    ScreenEntry registerClient(const std::string &instanceId,
                               const std::string &displayName,
                               const std::string &machineId,
                               int32_t width, int32_t height);

    /// Unregister a client
    void unregisterClient(const std::string &instanceId);

    /// Set client online/offline status
    void setClientOnline(const std::string &instanceId, bool online);

    /// Get the full layout
    std::vector<ScreenEntry> getLayout() const;

    /// Get a specific screen
    std::optional<ScreenEntry> getScreen(const std::string &instanceId) const;

    /// Get adjacency for a screen
    Adjacency getAdjacencyFor(const std::string &instanceId) const;

    /// Get transition target at an edge
    std::optional<TransitionTarget> getTransitionTargetAtEdge(
        const std::string &fromInstanceId,
        Side edge,
        int32_t x, int32_t y) const;

    /// Callback when layout changes
    std::function<void(const std::vector<ScreenEntry> &)> onLayoutChanged;

private:
    void notifyLayoutChanged();
    void arrangeScreens();

    std::unordered_map<std::string, ScreenEntry> m_screens;
    std::string m_serverInstanceId;
};

} // namespace konflikt
