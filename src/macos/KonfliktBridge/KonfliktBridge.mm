// Objective-C++ bridge implementation for libkonflikt

#import "KonfliktBridge.h"
#include <konflikt/ConfigManager.h>
#include <konflikt/Konflikt.h>
#include <memory>

#pragma mark - KonfliktConfig

@implementation KonfliktConfig

+ (instancetype)defaultConfig
{
    KonfliktConfig *config = [[KonfliktConfig alloc] init];
    config.role = KonfliktRoleServer;
    config.port = 3000;
    config.serverPort = 3000;
    config.screenX = 0;
    config.screenY = 0;
    config.screenWidth = 0;
    config.screenHeight = 0;
    config.edgeLeft = YES;
    config.edgeRight = YES;
    config.edgeTop = YES;
    config.edgeBottom = YES;
    config.lockCursorToScreen = NO;
    config.verbose = NO;
    return config;
}

+ (instancetype)loadFromFile
{
    auto cppConfig = konflikt::ConfigManager::load();
    if (!cppConfig) {
        return nil;
    }

    KonfliktConfig *config = [[KonfliktConfig alloc] init];
    config.role = (cppConfig->role == konflikt::InstanceRole::Server) ? KonfliktRoleServer : KonfliktRoleClient;
    config.port = cppConfig->port;
    config.serverPort = cppConfig->serverPort;
    config.screenX = cppConfig->screenX;
    config.screenY = cppConfig->screenY;
    config.screenWidth = cppConfig->screenWidth;
    config.screenHeight = cppConfig->screenHeight;
    config.edgeLeft = cppConfig->edgeLeft;
    config.edgeRight = cppConfig->edgeRight;
    config.edgeTop = cppConfig->edgeTop;
    config.edgeBottom = cppConfig->edgeBottom;
    config.lockCursorToScreen = cppConfig->lockCursorToScreen;
    config.verbose = cppConfig->verbose;

    if (!cppConfig->instanceId.empty()) {
        config.instanceId = [NSString stringWithUTF8String:cppConfig->instanceId.c_str()];
    }
    if (!cppConfig->instanceName.empty()) {
        config.instanceName = [NSString stringWithUTF8String:cppConfig->instanceName.c_str()];
    }
    if (!cppConfig->serverHost.empty()) {
        config.serverHost = [NSString stringWithUTF8String:cppConfig->serverHost.c_str()];
    }
    if (!cppConfig->uiPath.empty()) {
        config.uiPath = [NSString stringWithUTF8String:cppConfig->uiPath.c_str()];
    }
    if (!cppConfig->logFile.empty()) {
        config.logFile = [NSString stringWithUTF8String:cppConfig->logFile.c_str()];
    }

    return config;
}

@end

#pragma mark - Konflikt

@interface Konflikt () {
    std::unique_ptr<konflikt::Konflikt> _impl;
    konflikt::Config _config;
}

@end

@implementation Konflikt

- (instancetype)initWithConfig:(KonfliktConfig *)config
{
    self = [super init];
    if (self) {
        // Convert ObjC config to C++ config
        _config.role = (config.role == KonfliktRoleServer) ? konflikt::InstanceRole::Server : konflikt::InstanceRole::Client;
        _config.port = config.port;
        _config.serverPort = config.serverPort;
        _config.screenX = config.screenX;
        _config.screenY = config.screenY;
        _config.screenWidth = config.screenWidth;
        _config.screenHeight = config.screenHeight;
        _config.edgeLeft = config.edgeLeft;
        _config.edgeRight = config.edgeRight;
        _config.edgeTop = config.edgeTop;
        _config.edgeBottom = config.edgeBottom;
        _config.lockCursorToScreen = config.lockCursorToScreen;
        _config.verbose = config.verbose;

        if (config.instanceId) {
            _config.instanceId = [config.instanceId UTF8String];
        }
        if (config.instanceName) {
            _config.instanceName = [config.instanceName UTF8String];
        }
        if (config.serverHost) {
            _config.serverHost = [config.serverHost UTF8String];
        }
        if (config.uiPath) {
            _config.uiPath = [config.uiPath UTF8String];
        }
        if (config.logFile) {
            _config.logFile = [config.logFile UTF8String];
        }

        _impl = std::make_unique<konflikt::Konflikt>(_config);

        // Set up callbacks
        __weak Konflikt *weakSelf = self;

        _impl->setStatusCallback([weakSelf](konflikt::ConnectionStatus status, const std::string &message) {
            Konflikt *strongSelf = weakSelf;
            if (!strongSelf)
                return;

            KonfliktConnectionStatus objcStatus;
            switch (status) {
                case konflikt::ConnectionStatus::Disconnected:
                    objcStatus = KonfliktConnectionStatusDisconnected;
                    break;
                case konflikt::ConnectionStatus::Connecting:
                    objcStatus = KonfliktConnectionStatusConnecting;
                    break;
                case konflikt::ConnectionStatus::Connected:
                    objcStatus = KonfliktConnectionStatusConnected;
                    break;
                case konflikt::ConnectionStatus::Error:
                    objcStatus = KonfliktConnectionStatusError;
                    break;
            }

            NSString *msg = [NSString stringWithUTF8String:message.c_str()];

            dispatch_async(dispatch_get_main_queue(), ^{
                if ([strongSelf.delegate respondsToSelector:@selector(konflikt:didUpdateStatus:message:)]) {
                    [strongSelf.delegate konflikt:strongSelf didUpdateStatus:objcStatus message:msg];
                }
            });
        });

        _impl->setLogCallback([weakSelf](const std::string &level, const std::string &message) {
            Konflikt *strongSelf = weakSelf;
            if (!strongSelf)
                return;

            NSString *levelStr = [NSString stringWithUTF8String:level.c_str()];
            NSString *msgStr = [NSString stringWithUTF8String:message.c_str()];

            dispatch_async(dispatch_get_main_queue(), ^{
                if ([strongSelf.delegate respondsToSelector:@selector(konflikt:didLog:message:)]) {
                    [strongSelf.delegate konflikt:strongSelf didLog:levelStr message:msgStr];
                }
            });
        });
    }
    return self;
}

- (KonfliktConnectionStatus)connectionStatus
{
    switch (_impl->connectionStatus()) {
        case konflikt::ConnectionStatus::Disconnected:
            return KonfliktConnectionStatusDisconnected;
        case konflikt::ConnectionStatus::Connecting:
            return KonfliktConnectionStatusConnecting;
        case konflikt::ConnectionStatus::Connected:
            return KonfliktConnectionStatusConnected;
        case konflikt::ConnectionStatus::Error:
            return KonfliktConnectionStatusError;
    }
}

- (KonfliktRole)role
{
    return (_impl->role() == konflikt::InstanceRole::Server) ? KonfliktRoleServer : KonfliktRoleClient;
}

- (int)httpPort
{
    return _impl->httpPort();
}

- (NSString *)connectedServerName
{
    const std::string &name = _impl->connectedServerName();
    if (name.empty()) {
        return nil;
    }
    return [NSString stringWithUTF8String:name.c_str()];
}

- (BOOL)initialize
{
    return _impl->init() ? YES : NO;
}

- (void)run
{
    _impl->run();
}

- (void)stop
{
    _impl->stop();
}

- (void)quit
{
    _impl->quit();
}

- (void)setLockCursorToScreen:(BOOL)locked
{
    _impl->setLockCursorToScreen(locked);
}

- (BOOL)isLockCursorToScreen
{
    return _impl->isLockCursorToScreen() ? YES : NO;
}

- (void)setEdgeLeft:(BOOL)left right:(BOOL)right top:(BOOL)top bottom:(BOOL)bottom
{
    _impl->setEdgeTransitions(left, right, top, bottom);
}

- (BOOL)edgeLeft
{
    return _impl->edgeLeft() ? YES : NO;
}

- (BOOL)edgeRight
{
    return _impl->edgeRight() ? YES : NO;
}

- (BOOL)edgeTop
{
    return _impl->edgeTop() ? YES : NO;
}

- (BOOL)edgeBottom
{
    return _impl->edgeBottom() ? YES : NO;
}

- (BOOL)saveConfig
{
    return _impl->saveConfig() ? YES : NO;
}

@end
