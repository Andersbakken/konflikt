// macOS service discovery implementation using dns_sd (Bonjour)

#ifdef __APPLE__

#include <konflikt/ServiceDiscovery.h>

#include <arpa/inet.h>
#include <dns_sd.h>
#include <mutex>
#include <netdb.h>
#include <unordered_map>

namespace konflikt {

// Service type for Konflikt
static const char *kServiceType = "_konflikt._tcp";

struct ServiceDiscovery::Impl
{
    DNSServiceRef registerRef { nullptr };
    DNSServiceRef browseRef { nullptr };

    // Services being resolved
    std::unordered_map<std::string, DNSServiceRef> resolveRefs;

    // Discovered services
    std::unordered_map<std::string, DiscoveredService> services;

    mutable std::mutex mutex;

    // Registration info
    std::string registeredName;
    int registeredPort { 0 };
    std::string registeredInstanceId;

    // Parent pointer for callbacks
    ServiceDiscovery *parent { nullptr };

    // Callback helpers (called from static callbacks)
    void onRegisterResult(DNSServiceErrorType errorCode, const char *name);
    void onBrowseResult(DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode,
                        const char *serviceName, const char *replyDomain);
    void onResolveResult(DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget, uint16_t port,
                         uint16_t txtLen, const unsigned char *txtRecord);
};

// Static callback wrappers
static void DNSSD_API registerCallback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
                                       const char *name, const char *regtype, const char *domain, void *context)
{
    (void)sdRef;
    (void)flags;
    (void)regtype;
    (void)domain;

    auto *impl = static_cast<ServiceDiscovery::Impl *>(context);
    impl->onRegisterResult(errorCode, name);
}

static void DNSSD_API browseCallback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                     DNSServiceErrorType errorCode, const char *serviceName, const char *regtype,
                                     const char *replyDomain, void *context)
{
    (void)sdRef;
    (void)regtype;

    auto *impl = static_cast<ServiceDiscovery::Impl *>(context);
    impl->onBrowseResult(flags, interfaceIndex, errorCode, serviceName, replyDomain);
}

static void DNSSD_API resolveCallback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                      DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget,
                                      uint16_t port, uint16_t txtLen, const unsigned char *txtRecord, void *context)
{
    (void)sdRef;
    (void)flags;
    (void)interfaceIndex;

    auto *impl = static_cast<ServiceDiscovery::Impl *>(context);
    impl->onResolveResult(errorCode, fullname, hosttarget, port, txtLen, txtRecord);
}

void ServiceDiscovery::Impl::onRegisterResult(DNSServiceErrorType errorCode, const char *name)
{
    (void)name;

    if (errorCode != kDNSServiceErr_NoError) {
        if (parent->mCallbacks.onError) {
            parent->mCallbacks.onError("Service registration failed: " + std::to_string(errorCode));
        }
    }
}

void ServiceDiscovery::Impl::onBrowseResult(DNSServiceFlags flags, uint32_t interfaceIndex,
                                            DNSServiceErrorType errorCode, const char *serviceName,
                                            const char *replyDomain)
{
    if (errorCode != kDNSServiceErr_NoError) {
        if (parent->mCallbacks.onError) {
            parent->mCallbacks.onError("Browse error: " + std::to_string(errorCode));
        }
        return;
    }

    std::string name(serviceName);

    if (flags & kDNSServiceFlagsAdd) {
        // Service found - need to resolve to get host and port
        std::lock_guard<std::mutex> lock(mutex);

        // Check if already resolving
        if (resolveRefs.count(name) > 0) {
            return;
        }

        DNSServiceRef resolveRef = nullptr;
        DNSServiceErrorType err =
            DNSServiceResolve(&resolveRef, 0, interfaceIndex, serviceName, kServiceType, replyDomain,
                              konflikt::resolveCallback, this);

        if (err == kDNSServiceErr_NoError) {
            resolveRefs[name] = resolveRef;
        }
    } else {
        // Service removed
        std::lock_guard<std::mutex> lock(mutex);

        // Cancel any pending resolve
        auto it = resolveRefs.find(name);
        if (it != resolveRefs.end()) {
            DNSServiceRefDeallocate(it->second);
            resolveRefs.erase(it);
        }

        // Remove from discovered services
        services.erase(name);

        if (parent->mCallbacks.onServiceLost) {
            parent->mCallbacks.onServiceLost(name);
        }
    }
}

void ServiceDiscovery::Impl::onResolveResult(DNSServiceErrorType errorCode, const char *fullname,
                                             const char *hosttarget, uint16_t port, uint16_t txtLen,
                                             const unsigned char *txtRecord)
{
    if (errorCode != kDNSServiceErr_NoError) {
        if (parent->mCallbacks.onError) {
            parent->mCallbacks.onError("Resolve error: " + std::to_string(errorCode));
        }
        return;
    }

    // Extract service name from fullname (format: "Name._konflikt._tcp.local.")
    std::string fullnameStr(fullname);
    std::string name;
    size_t pos = fullnameStr.find("._konflikt");
    if (pos != std::string::npos) {
        name = fullnameStr.substr(0, pos);
    } else {
        name = fullnameStr;
    }

    // Parse TXT record for instance ID
    std::string instanceId;
    if (txtLen > 0 && txtRecord) {
        const unsigned char *ptr = txtRecord;
        const unsigned char *end = txtRecord + txtLen;

        while (ptr < end) {
            uint8_t len = *ptr++;
            if (ptr + len > end)
                break;

            std::string entry(reinterpret_cast<const char *>(ptr), len);
            ptr += len;

            // Parse key=value
            size_t eq = entry.find('=');
            if (eq != std::string::npos) {
                std::string key = entry.substr(0, eq);
                std::string value = entry.substr(eq + 1);
                if (key == "id") {
                    instanceId = value;
                }
            }
        }
    }

    DiscoveredService service;
    service.name = name;
    service.host = hosttarget;
    service.port = ntohs(port);
    service.instanceId = instanceId;

    {
        std::lock_guard<std::mutex> lock(mutex);
        services[name] = service;

        // Clean up resolve ref
        auto it = resolveRefs.find(name);
        if (it != resolveRefs.end()) {
            DNSServiceRefDeallocate(it->second);
            resolveRefs.erase(it);
        }
    }

    if (parent->mCallbacks.onServiceFound) {
        parent->mCallbacks.onServiceFound(service);
    }
}

ServiceDiscovery::ServiceDiscovery()
    : mImpl(std::make_unique<Impl>())
{
    mImpl->parent = this;
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
    if (mRegistered) {
        unregisterService();
    }

    // Create TXT record with instance ID
    TXTRecordRef txtRecord;
    TXTRecordCreate(&txtRecord, 0, nullptr);
    TXTRecordSetValue(&txtRecord, "id", static_cast<uint8_t>(instanceId.length()), instanceId.c_str());

    DNSServiceErrorType err = DNSServiceRegister(&mImpl->registerRef, 0, // flags
                                                 0,                       // interface index (0 = all)
                                                 name.c_str(),            // service name
                                                 kServiceType,            // service type
                                                 nullptr,                 // domain (null = default)
                                                 nullptr,                 // host (null = this machine)
                                                 htons(static_cast<uint16_t>(port)), TXTRecordGetLength(&txtRecord),
                                                 TXTRecordGetBytesPtr(&txtRecord), registerCallback, mImpl.get());

    TXTRecordDeallocate(&txtRecord);

    if (err != kDNSServiceErr_NoError) {
        if (mCallbacks.onError) {
            mCallbacks.onError("Failed to register service: " + std::to_string(err));
        }
        return false;
    }

    mImpl->registeredName = name;
    mImpl->registeredPort = port;
    mImpl->registeredInstanceId = instanceId;
    mRegistered = true;

    return true;
}

void ServiceDiscovery::unregisterService()
{
    if (mImpl->registerRef) {
        DNSServiceRefDeallocate(mImpl->registerRef);
        mImpl->registerRef = nullptr;
    }
    mRegistered = false;
}

bool ServiceDiscovery::startBrowsing()
{
    if (mBrowsing) {
        return true;
    }

    DNSServiceErrorType err = DNSServiceBrowse(&mImpl->browseRef, 0, // flags
                                               0,                     // interface index (0 = all)
                                               kServiceType,          // service type
                                               nullptr,               // domain (null = default)
                                               browseCallback, mImpl.get());

    if (err != kDNSServiceErr_NoError) {
        if (mCallbacks.onError) {
            mCallbacks.onError("Failed to start browsing: " + std::to_string(err));
        }
        return false;
    }

    mBrowsing = true;
    return true;
}

void ServiceDiscovery::stopBrowsing()
{
    std::lock_guard<std::mutex> lock(mImpl->mutex);

    // Clean up all resolve refs
    for (auto &[name, ref] : mImpl->resolveRefs) {
        DNSServiceRefDeallocate(ref);
    }
    mImpl->resolveRefs.clear();

    // Clean up browse ref
    if (mImpl->browseRef) {
        DNSServiceRefDeallocate(mImpl->browseRef);
        mImpl->browseRef = nullptr;
    }

    mImpl->services.clear();
    mBrowsing = false;
}

void ServiceDiscovery::poll()
{
    // Process registration events
    if (mImpl->registerRef) {
        int fd = DNSServiceRefSockFD(mImpl->registerRef);
        if (fd >= 0) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            struct timeval tv = { 0, 0 }; // Non-blocking
            if (select(fd + 1, &readfds, nullptr, nullptr, &tv) > 0) {
                DNSServiceProcessResult(mImpl->registerRef);
            }
        }
    }

    // Process browse events
    if (mImpl->browseRef) {
        int fd = DNSServiceRefSockFD(mImpl->browseRef);
        if (fd >= 0) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            struct timeval tv = { 0, 0 }; // Non-blocking
            if (select(fd + 1, &readfds, nullptr, nullptr, &tv) > 0) {
                DNSServiceProcessResult(mImpl->browseRef);
            }
        }
    }

    // Process resolve events
    {
        std::lock_guard<std::mutex> lock(mImpl->mutex);
        for (auto &[name, ref] : mImpl->resolveRefs) {
            int fd = DNSServiceRefSockFD(ref);
            if (fd >= 0) {
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(fd, &readfds);

                struct timeval tv = { 0, 0 }; // Non-blocking
                if (select(fd + 1, &readfds, nullptr, nullptr, &tv) > 0) {
                    DNSServiceProcessResult(ref);
                }
            }
        }
    }
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

#endif // __APPLE__
