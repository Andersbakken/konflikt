#include "konflikt/LayoutManager.h"

#include <algorithm>

namespace konflikt {

LayoutManager::LayoutManager() = default;
LayoutManager::~LayoutManager() = default;

void LayoutManager::setServerScreen(const std::string &instanceId,
                                    const std::string &displayName,
                                    const std::string &machineId,
                                    int32_t width, int32_t height)
{
    m_serverInstanceId = instanceId;

    ScreenEntry entry;
    entry.instanceId = instanceId;
    entry.displayName = displayName;
    entry.machineId = machineId;
    entry.x = 0;
    entry.y = 0;
    entry.width = width;
    entry.height = height;
    entry.isServer = true;
    entry.online = true;

    m_screens[instanceId] = entry;
    notifyLayoutChanged();
}

ScreenEntry LayoutManager::registerClient(const std::string &instanceId,
                                          const std::string &displayName,
                                          const std::string &machineId,
                                          int32_t width, int32_t height)
{
    ScreenEntry entry;
    entry.instanceId = instanceId;
    entry.displayName = displayName;
    entry.machineId = machineId;
    entry.width = width;
    entry.height = height;
    entry.isServer = false;
    entry.online = true;

    // Position the client screen to the right of the server
    // Find the rightmost screen
    int32_t maxRight = 0;
    for (const auto &[id, screen] : m_screens) {
        maxRight = std::max(maxRight, screen.x + screen.width);
    }
    entry.x = maxRight;
    entry.y = 0;

    m_screens[instanceId] = entry;
    notifyLayoutChanged();

    return entry;
}

void LayoutManager::unregisterClient(const std::string &instanceId)
{
    m_screens.erase(instanceId);
    arrangeScreens();
    notifyLayoutChanged();
}

void LayoutManager::setClientOnline(const std::string &instanceId, bool online)
{
    auto it = m_screens.find(instanceId);
    if (it != m_screens.end()) {
        it->second.online = online;
        notifyLayoutChanged();
    }
}

std::vector<ScreenEntry> LayoutManager::getLayout() const
{
    std::vector<ScreenEntry> layout;
    layout.reserve(m_screens.size());
    for (const auto &[id, screen] : m_screens) {
        layout.push_back(screen);
    }

    // Sort by x position (left to right)
    std::sort(layout.begin(), layout.end(), [](const ScreenEntry &a, const ScreenEntry &b) {
        return a.x < b.x;
    });

    return layout;
}

std::optional<ScreenEntry> LayoutManager::getScreen(const std::string &instanceId) const
{
    auto it = m_screens.find(instanceId);
    if (it != m_screens.end()) {
        return it->second;
    }
    return std::nullopt;
}

Adjacency LayoutManager::getAdjacencyFor(const std::string &instanceId) const
{
    Adjacency adj;

    auto it = m_screens.find(instanceId);
    if (it == m_screens.end()) {
        return adj;
    }

    const auto &screen = it->second;

    // Find adjacent screens
    for (const auto &[id, other] : m_screens) {
        if (id == instanceId)
            continue;

        // Left neighbor: other's right edge touches this screen's left edge
        if (other.x + other.width == screen.x) {
            adj.left = id;
        }
        // Right neighbor: this screen's right edge touches other's left edge
        if (screen.x + screen.width == other.x) {
            adj.right = id;
        }
        // Top neighbor: other's bottom edge touches this screen's top edge
        if (other.y + other.height == screen.y) {
            adj.top = id;
        }
        // Bottom neighbor: this screen's bottom edge touches other's top edge
        if (screen.y + screen.height == other.y) {
            adj.bottom = id;
        }
    }

    return adj;
}

std::optional<TransitionTarget> LayoutManager::getTransitionTargetAtEdge(
    const std::string &fromInstanceId,
    Side edge,
    int32_t x, int32_t y) const
{
    auto fromIt = m_screens.find(fromInstanceId);
    if (fromIt == m_screens.end()) {
        return std::nullopt;
    }

    const auto &fromScreen = fromIt->second;
    Adjacency adj = getAdjacencyFor(fromInstanceId);

    std::optional<std::string> targetId;

    switch (edge) {
        case Side::Left:
            targetId = adj.left;
            break;
        case Side::Right:
            targetId = adj.right;
            break;
        case Side::Top:
            targetId = adj.top;
            break;
        case Side::Bottom:
            targetId = adj.bottom;
            break;
    }

    if (!targetId) {
        return std::nullopt;
    }

    auto targetIt = m_screens.find(*targetId);
    if (targetIt == m_screens.end() || !targetIt->second.online) {
        return std::nullopt;
    }

    const auto &targetScreen = targetIt->second;

    TransitionTarget target;
    target.targetScreen = targetScreen;

    // Calculate new cursor position on the target screen
    switch (edge) {
        case Side::Left:
            // Coming from right edge of target screen
            target.newX = targetScreen.width - 2;
            target.newY = std::clamp(y - fromScreen.y, 0, targetScreen.height - 1);
            break;
        case Side::Right:
            // Coming to left edge of target screen
            target.newX = 1;
            target.newY = std::clamp(y - fromScreen.y, 0, targetScreen.height - 1);
            break;
        case Side::Top:
            // Coming from bottom edge of target screen
            target.newX = std::clamp(x - fromScreen.x, 0, targetScreen.width - 1);
            target.newY = targetScreen.height - 2;
            break;
        case Side::Bottom:
            // Coming to top edge of target screen
            target.newX = std::clamp(x - fromScreen.x, 0, targetScreen.width - 1);
            target.newY = 1;
            break;
    }

    return target;
}

void LayoutManager::notifyLayoutChanged()
{
    if (onLayoutChanged) {
        onLayoutChanged(getLayout());
    }
}

void LayoutManager::arrangeScreens()
{
    // Re-arrange screens left to right after one is removed
    std::vector<std::pair<std::string, ScreenEntry *>> screens;
    for (auto &[id, screen] : m_screens) {
        screens.emplace_back(id, &screen);
    }

    // Sort by original x position
    std::sort(screens.begin(), screens.end(), [](const auto &a, const auto &b) {
        return a.second->x < b.second->x;
    });

    // Reposition
    int32_t currentX = 0;
    for (auto &[id, screen] : screens) {
        screen->x = currentX;
        screen->y = 0;
        currentX += screen->width;
    }
}

} // namespace konflikt
