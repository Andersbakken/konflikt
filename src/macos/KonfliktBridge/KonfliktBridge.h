// Objective-C++ bridge for libkonflikt
// This provides a Swift-friendly interface to the C++ library

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Instance role
typedef NS_ENUM(NSInteger, KonfliktRole) {
    KonfliktRoleServer,
    KonfliktRoleClient
};

/// Connection status
typedef NS_ENUM(NSInteger, KonfliktConnectionStatus) {
    KonfliktConnectionStatusDisconnected,
    KonfliktConnectionStatusConnecting,
    KonfliktConnectionStatusConnected,
    KonfliktConnectionStatusError
};

/// Configuration for Konflikt instance
@interface KonfliktConfig : NSObject

@property(nonatomic) KonfliktRole role;
@property(nonatomic, copy, nullable) NSString *instanceId;
@property(nonatomic, copy, nullable) NSString *instanceName;
@property(nonatomic) int port;
@property(nonatomic, copy, nullable) NSString *serverHost;
@property(nonatomic) int serverPort;
@property(nonatomic) int screenX;
@property(nonatomic) int screenY;
@property(nonatomic) int screenWidth;
@property(nonatomic) int screenHeight;
@property(nonatomic) BOOL edgeLeft;
@property(nonatomic) BOOL edgeRight;
@property(nonatomic) BOOL edgeTop;
@property(nonatomic) BOOL edgeBottom;
@property(nonatomic) BOOL lockCursorToScreen;
@property(nonatomic, copy, nullable) NSString *uiPath;
@property(nonatomic) BOOL verbose;
@property(nonatomic, copy, nullable) NSString *logFile;

+ (instancetype)defaultConfig;

@end

/// Delegate for Konflikt status updates
@protocol KonfliktDelegate <NSObject>

@optional
- (void)konflikt:(id)konflikt didUpdateStatus:(KonfliktConnectionStatus)status message:(NSString *)message;
- (void)konflikt:(id)konflikt didLog:(NSString *)level message:(NSString *)message;

@end

/// Main Konflikt wrapper class
@interface Konflikt : NSObject

@property(nonatomic, weak, nullable) id<KonfliktDelegate> delegate;
@property(nonatomic, readonly) KonfliktConnectionStatus connectionStatus;
@property(nonatomic, readonly) KonfliktRole role;
@property(nonatomic, readonly) int httpPort;
@property(nonatomic, readonly, copy, nullable) NSString *connectedServerName;

- (instancetype)initWithConfig:(KonfliktConfig *)config;

/// Initialize the instance (call before start)
- (BOOL)initialize;

/// Run the main event loop (blocking - call on background thread)
- (void)run;

/// Stop the instance
- (void)stop;

/// Request quit (signal to stop running)
- (void)quit;

/// Lock/unlock cursor to current screen
- (void)setLockCursorToScreen:(BOOL)locked;
- (BOOL)isLockCursorToScreen;

@end

NS_ASSUME_NONNULL_END
