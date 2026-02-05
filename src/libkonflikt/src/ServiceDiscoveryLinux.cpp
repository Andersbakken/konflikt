// Linux service discovery implementation (Avahi)

#ifndef __APPLE__

#include <konflikt/ServiceDiscovery.h>

#ifdef HAVE_AVAHI

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/thread-watch.h>

#include <mutex>
#include <unordered_map>

namespace konflikt {

// Service type for Konflikt
static const char *kServiceType = "_konflikt._tcp";

struct ServiceDiscovery::Impl
{
    AvahiThreadedPoll *threadedPoll { nullptr };
    AvahiClient *client { nullptr };
    AvahiEntryGroup *entryGroup { nullptr };
    AvahiServiceBrowser *browser { nullptr };

    // Services being resolved
    std::unordered_map<std::string, AvahiServiceResolver *> resolvers;

    // Discovered services
    std::unordered_map<std::string, DiscoveredService> services;

    mutable std::mutex mutex;

    // Registration info
    std::string registeredName;
    int registeredPort { 0 };
    std::string registeredInstanceId;

    // Parent pointer for callbacks
    ServiceDiscovery *parent { nullptr };

    // Make unique key for service
    static std::string makeKey(const char *name, const char *type, const char *domain)
    {
        return std::string(name) + "." + type + "." + domain;
    }

    // Callback helpers
    void onClientStateChange(AvahiClientState state);
    void onEntryGroupStateChange(AvahiEntryGroupState state);
    void onBrowseResult(AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event,
                        const char *name, const char *type, const char *domain);
    void onResolveResult(AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
                         const char *name, const char *type, const char *domain,
                         const char *hostName, const AvahiAddress *address, uint16_t port,
                         AvahiStringList *txt);
};

// Static callback wrappers
static void clientCallback(AvahiClient *c, AvahiClientState state, void *userdata)
{
    (void)c;
    auto *impl = static_cast<ServiceDiscovery::Impl *>(userdata);
    impl->onClientStateChange(state);
}

static void entryGroupCallback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata)
{
    (void)g;
    auto *impl = static_cast<ServiceDiscovery::Impl *>(userdata);
    impl->onEntryGroupStateChange(state);
}

static void browseCallback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol,
                           AvahiBrowserEvent event, const char *name, const char *type,
                           const char *domain, AvahiLookupResultFlags flags, void *userdata)
{
    (void)b;
    (void)flags;
    auto *impl = static_cast<ServiceDiscovery::Impl *>(userdata);
    impl->onBrowseResult(interface, protocol, event, name, type, domain);
}

static void resolveCallback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol,
                            AvahiResolverEvent event, const char *name, const char *type,
                            const char *domain, const char *hostName, const AvahiAddress *address,
                            uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags,
                            void *userdata)
{
    (void)r;
    (void)flags;
    auto *impl = static_cast<ServiceDiscovery::Impl *>(userdata);
    impl->onResolveResult(interface, protocol, event, name, type, domain, hostName, address, port, txt);
}

void ServiceDiscovery::Impl::onClientStateChange(AvahiClientState state)
{
    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            // Server is running, register services if needed
            if (!registeredName.empty() && !entryGroup && client) {
                entryGroup = avahi_entry_group_new(client, entryGroupCallback, this);
                if (entryGroup) {
                    // Create TXT record with instance ID
                    std::string txtRecord = "id=" + registeredInstanceId;

                    int ret = avahi_entry_group_add_service(
                        entryGroup,
                        AVAHI_IF_UNSPEC,
                        AVAHI_PROTO_UNSPEC,
                        static_cast<AvahiPublishFlags>(0),
                        registeredName.c_str(),
                        kServiceType,
                        nullptr,  // domain
                        nullptr,  // host
                        static_cast<uint16_t>(registeredPort),
                        txtRecord.c_str(),
                        nullptr);

                    if (ret < 0) {
                        if (parent->mCallbacks.onError) {
                            parent->mCallbacks.onError("Failed to add service: " + std::string(avahi_strerror(ret)));
                        }
                    } else {
                        avahi_entry_group_commit(entryGroup);
                    }
                }
            }
            break;

        case AVAHI_CLIENT_FAILURE:
            if (parent->mCallbacks.onError) {
                parent->mCallbacks.onError("Avahi client failure: " +
                                           std::string(avahi_strerror(avahi_client_errno(client))));
            }
            break;

        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:
            // Reset entry group on collision
            if (entryGroup) {
                avahi_entry_group_reset(entryGroup);
            }
            break;

        case AVAHI_CLIENT_CONNECTING:
            break;
    }
}

void ServiceDiscovery::Impl::onEntryGroupStateChange(AvahiEntryGroupState state)
{
    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            parent->mRegistered = true;
            break;

        case AVAHI_ENTRY_GROUP_COLLISION:
            // Name collision, need to pick new name
            if (parent->mCallbacks.onError) {
                parent->mCallbacks.onError("Service name collision");
            }
            break;

        case AVAHI_ENTRY_GROUP_FAILURE:
            if (parent->mCallbacks.onError) {
                parent->mCallbacks.onError("Entry group failure: " +
                                           std::string(avahi_strerror(avahi_client_errno(client))));
            }
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            break;
    }
}

void ServiceDiscovery::Impl::onBrowseResult(AvahiIfIndex interface, AvahiProtocol protocol,
                                            AvahiBrowserEvent event, const char *name,
                                            const char *type, const char *domain)
{
    std::string key = makeKey(name, type, domain);

    switch (event) {
        case AVAHI_BROWSER_NEW: {
            std::lock_guard<std::mutex> lock(mutex);

            // Check if already resolving
            if (resolvers.count(key) > 0) {
                return;
            }

            // Start resolving
            AvahiServiceResolver *resolver = avahi_service_resolver_new(
                client,
                interface,
                protocol,
                name,
                type,
                domain,
                AVAHI_PROTO_UNSPEC,
                static_cast<AvahiLookupFlags>(0),
                resolveCallback,
                this);

            if (resolver) {
                resolvers[key] = resolver;
            }
            break;
        }

        case AVAHI_BROWSER_REMOVE: {
            std::lock_guard<std::mutex> lock(mutex);

            // Cancel any pending resolve
            auto it = resolvers.find(key);
            if (it != resolvers.end()) {
                avahi_service_resolver_free(it->second);
                resolvers.erase(it);
            }

            // Remove from discovered services
            auto sit = services.find(name);
            if (sit != services.end()) {
                services.erase(sit);

                if (parent->mCallbacks.onServiceLost) {
                    parent->mCallbacks.onServiceLost(name);
                }
            }
            break;
        }

        case AVAHI_BROWSER_CACHE_EXHAUSTED:
        case AVAHI_BROWSER_ALL_FOR_NOW:
            break;

        case AVAHI_BROWSER_FAILURE:
            if (parent->mCallbacks.onError) {
                parent->mCallbacks.onError("Browser failure: " +
                                           std::string(avahi_strerror(avahi_client_errno(client))));
            }
            break;
    }
}

void ServiceDiscovery::Impl::onResolveResult(AvahiIfIndex interface, AvahiProtocol protocol,
                                             AvahiResolverEvent event, const char *name,
                                             const char *type, const char *domain,
                                             const char *hostName, const AvahiAddress *address,
                                             uint16_t port, AvahiStringList *txt)
{
    (void)interface;
    (void)protocol;
    (void)address;

    std::string key = makeKey(name, type, domain);

    switch (event) {
        case AVAHI_RESOLVER_FOUND: {
            // Parse TXT record for instance ID
            std::string instanceId;
            for (AvahiStringList *l = txt; l; l = avahi_string_list_get_next(l)) {
                char *k = nullptr;
                char *v = nullptr;
                if (avahi_string_list_get_pair(l, &k, &v, nullptr) >= 0) {
                    if (k && v && std::string(k) == "id") {
                        instanceId = v;
                    }
                    avahi_free(k);
                    avahi_free(v);
                }
            }

            DiscoveredService service;
            service.name = name;
            service.host = hostName;
            service.port = port;
            service.instanceId = instanceId;

            {
                std::lock_guard<std::mutex> lock(mutex);
                services[name] = service;

                // Clean up resolver
                auto it = resolvers.find(key);
                if (it != resolvers.end()) {
                    avahi_service_resolver_free(it->second);
                    resolvers.erase(it);
                }
            }

            if (parent->mCallbacks.onServiceFound) {
                parent->mCallbacks.onServiceFound(service);
            }
            break;
        }

        case AVAHI_RESOLVER_FAILURE:
            if (parent->mCallbacks.onError) {
                parent->mCallbacks.onError("Resolver failure: " +
                                           std::string(avahi_strerror(avahi_client_errno(client))));
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                auto it = resolvers.find(key);
                if (it != resolvers.end()) {
                    avahi_service_resolver_free(it->second);
                    resolvers.erase(it);
                }
            }
            break;
    }
}

ServiceDiscovery::ServiceDiscovery()
    : mImpl(std::make_unique<Impl>())
{
    mImpl->parent = this;

    // Create threaded poll
    mImpl->threadedPoll = avahi_threaded_poll_new();
    if (!mImpl->threadedPoll) {
        return;
    }

    // Create client
    int error = 0;
    mImpl->client = avahi_client_new(
        avahi_threaded_poll_get(mImpl->threadedPoll),
        static_cast<AvahiClientFlags>(0),
        clientCallback,
        mImpl.get(),
        &error);

    if (!mImpl->client) {
        avahi_threaded_poll_free(mImpl->threadedPoll);
        mImpl->threadedPoll = nullptr;
        return;
    }

    // Start the threaded poll
    avahi_threaded_poll_start(mImpl->threadedPoll);
}

ServiceDiscovery::~ServiceDiscovery()
{
    unregisterService();
    stopBrowsing();

    if (mImpl->threadedPoll) {
        avahi_threaded_poll_stop(mImpl->threadedPoll);
    }

    if (mImpl->client) {
        avahi_client_free(mImpl->client);
    }

    if (mImpl->threadedPoll) {
        avahi_threaded_poll_free(mImpl->threadedPoll);
    }
}

void ServiceDiscovery::setCallbacks(ServiceDiscoveryCallbacks callbacks)
{
    mCallbacks = std::move(callbacks);
}

bool ServiceDiscovery::registerService(const std::string &name, int port, const std::string &instanceId)
{
    if (!mImpl->client) {
        if (mCallbacks.onError) {
            mCallbacks.onError("Avahi client not initialized");
        }
        return false;
    }

    if (mRegistered) {
        unregisterService();
    }

    avahi_threaded_poll_lock(mImpl->threadedPoll);

    mImpl->registeredName = name;
    mImpl->registeredPort = port;
    mImpl->registeredInstanceId = instanceId;

    // If client is already running, create entry group now
    if (avahi_client_get_state(mImpl->client) == AVAHI_CLIENT_S_RUNNING) {
        mImpl->entryGroup = avahi_entry_group_new(mImpl->client, entryGroupCallback, mImpl.get());
        if (mImpl->entryGroup) {
            std::string txtRecord = "id=" + instanceId;

            int ret = avahi_entry_group_add_service(
                mImpl->entryGroup,
                AVAHI_IF_UNSPEC,
                AVAHI_PROTO_UNSPEC,
                static_cast<AvahiPublishFlags>(0),
                name.c_str(),
                kServiceType,
                nullptr,
                nullptr,
                static_cast<uint16_t>(port),
                txtRecord.c_str(),
                nullptr);

            if (ret < 0) {
                avahi_threaded_poll_unlock(mImpl->threadedPoll);
                if (mCallbacks.onError) {
                    mCallbacks.onError("Failed to add service: " + std::string(avahi_strerror(ret)));
                }
                return false;
            }

            avahi_entry_group_commit(mImpl->entryGroup);
        }
    }

    avahi_threaded_poll_unlock(mImpl->threadedPoll);
    return true;
}

void ServiceDiscovery::unregisterService()
{
    if (!mImpl->threadedPoll) {
        return;
    }

    avahi_threaded_poll_lock(mImpl->threadedPoll);

    if (mImpl->entryGroup) {
        avahi_entry_group_reset(mImpl->entryGroup);
        avahi_entry_group_free(mImpl->entryGroup);
        mImpl->entryGroup = nullptr;
    }

    mImpl->registeredName.clear();
    mRegistered = false;

    avahi_threaded_poll_unlock(mImpl->threadedPoll);
}

bool ServiceDiscovery::startBrowsing()
{
    if (!mImpl->client) {
        if (mCallbacks.onError) {
            mCallbacks.onError("Avahi client not initialized");
        }
        return false;
    }

    if (mBrowsing) {
        return true;
    }

    avahi_threaded_poll_lock(mImpl->threadedPoll);

    mImpl->browser = avahi_service_browser_new(
        mImpl->client,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        kServiceType,
        nullptr,
        static_cast<AvahiLookupFlags>(0),
        browseCallback,
        mImpl.get());

    avahi_threaded_poll_unlock(mImpl->threadedPoll);

    if (!mImpl->browser) {
        if (mCallbacks.onError) {
            mCallbacks.onError("Failed to create service browser: " +
                               std::string(avahi_strerror(avahi_client_errno(mImpl->client))));
        }
        return false;
    }

    mBrowsing = true;
    return true;
}

void ServiceDiscovery::stopBrowsing()
{
    if (!mImpl->threadedPoll) {
        return;
    }

    avahi_threaded_poll_lock(mImpl->threadedPoll);

    // Clean up all resolvers
    for (auto &[name, resolver] : mImpl->resolvers) {
        avahi_service_resolver_free(resolver);
    }
    mImpl->resolvers.clear();

    // Clean up browser
    if (mImpl->browser) {
        avahi_service_browser_free(mImpl->browser);
        mImpl->browser = nullptr;
    }

    mImpl->services.clear();
    mBrowsing = false;

    avahi_threaded_poll_unlock(mImpl->threadedPoll);
}

void ServiceDiscovery::poll()
{
    // Avahi uses threaded poll, so no manual polling needed
}

std::vector<DiscoveredService> ServiceDiscovery::getDiscoveredServices() const
{
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    std::vector<DiscoveredService> result;
    result.reserve(mImpl->services.size());
    for (const auto &[name, service] : mImpl->services) {
        result.push_back(service);
    }
    return result;
}

} // namespace konflikt

#else // !HAVE_AVAHI

// Stub implementation when Avahi is not available
namespace konflikt {

struct ServiceDiscovery::Impl
{
    // Empty stub
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

    if (mCallbacks.onError) {
        mCallbacks.onError("Service discovery not available (Avahi not found). Use --server=host to connect manually.");
    }
    return false;
}

void ServiceDiscovery::unregisterService()
{
    mRegistered = false;
}

bool ServiceDiscovery::startBrowsing()
{
    if (mCallbacks.onError) {
        mCallbacks.onError("Service discovery not available (Avahi not found). Use --server=host to connect manually.");
    }
    return false;
}

void ServiceDiscovery::stopBrowsing()
{
    mBrowsing = false;
}

void ServiceDiscovery::poll()
{
}

std::vector<DiscoveredService> ServiceDiscovery::getDiscoveredServices() const
{
    return {};
}

} // namespace konflikt

#endif // HAVE_AVAHI

#endif // !__APPLE__
