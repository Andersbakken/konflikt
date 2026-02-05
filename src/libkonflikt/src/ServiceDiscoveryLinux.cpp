// Linux service discovery implementation (Avahi)
// TODO: Full Avahi implementation

#ifndef __APPLE__

#include <konflikt/ServiceDiscovery.h>

namespace konflikt {

// Stub implementation for Linux
// Full Avahi implementation can be added later

struct ServiceDiscovery::Impl
{
    // Will hold Avahi client and browser references
};

ServiceDiscovery::ServiceDiscovery()
    : mImpl(std::make_unique<Impl>())
{
}

ServiceDiscovery::~ServiceDiscovery()
{
    unregisterService();
    stopBrowsing();
}

void ServiceDiscovery::setCallbacks(ServiceDiscoveryCallbacks callbacks)
{
    mCallbacks = std::move(callbacks);
}

bool ServiceDiscovery::registerService(const std::string &name, int port, const std::string &instanceId)
{
    (void)name;
    (void)port;
    (void)instanceId;

    // TODO: Implement using Avahi
    // avahi_entry_group_add_service()
    if (mCallbacks.onError) {
        mCallbacks.onError("Service discovery not implemented on Linux (use --server=host)");
    }
    return false;
}

void ServiceDiscovery::unregisterService()
{
    mRegistered = false;
}

bool ServiceDiscovery::startBrowsing()
{
    // TODO: Implement using Avahi
    // avahi_service_browser_new()
    if (mCallbacks.onError) {
        mCallbacks.onError("Service discovery not implemented on Linux (use --server=host)");
    }
    return false;
}

void ServiceDiscovery::stopBrowsing()
{
    mBrowsing = false;
}

void ServiceDiscovery::poll()
{
    // TODO: Process Avahi events
}

std::vector<DiscoveredService> ServiceDiscovery::getDiscoveredServices() const
{
    return {};
}

} // namespace konflikt

#endif // !__APPLE__
