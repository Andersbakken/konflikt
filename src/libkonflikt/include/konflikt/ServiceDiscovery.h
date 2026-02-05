#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace konflikt {

/// Information about a discovered service
struct DiscoveredService
{
    std::string name;
    std::string host;
    int port;
    std::string instanceId;
};

/// Callbacks for service discovery events
struct ServiceDiscoveryCallbacks
{
    std::function<void(const DiscoveredService &service)> onServiceFound;
    std::function<void(const std::string &name)> onServiceLost;
    std::function<void(const std::string &error)> onError;
};

/// Service discovery using mDNS (Bonjour on macOS, Avahi on Linux)
class ServiceDiscovery
{
public:
    // Forward declaration of implementation (public for static callback access)
    struct Impl;

    ServiceDiscovery();
    ~ServiceDiscovery();

    // Non-copyable
    ServiceDiscovery(const ServiceDiscovery &) = delete;
    ServiceDiscovery &operator=(const ServiceDiscovery &) = delete;

    /// Set callbacks for discovery events
    void setCallbacks(ServiceDiscoveryCallbacks callbacks);

    /// Register a service (server mode)
    /// @param name Service display name
    /// @param port Port the service is running on
    /// @param instanceId Unique instance identifier
    /// @return true if registration started successfully
    bool registerService(const std::string &name, int port, const std::string &instanceId);

    /// Unregister the service
    void unregisterService();

    /// Start browsing for services (client mode)
    /// @return true if browsing started successfully
    bool startBrowsing();

    /// Stop browsing for services
    void stopBrowsing();

    /// Check if currently browsing
    bool isBrowsing() const { return mBrowsing; }

    /// Check if service is registered
    bool isRegistered() const { return mRegistered; }

    /// Process pending events (call periodically)
    void poll();

    /// Get list of currently discovered services
    std::vector<DiscoveredService> getDiscoveredServices() const;

private:
    std::unique_ptr<Impl> mImpl;

    ServiceDiscoveryCallbacks mCallbacks;
    bool mBrowsing { false };
    bool mRegistered { false };
};

} // namespace konflikt
