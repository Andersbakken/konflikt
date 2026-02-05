import Cocoa
import UserNotifications

/// Manages system notifications for Konflikt events
class NotificationManager: NSObject {
    static let shared = NotificationManager()

    private var isAuthorized = false

    private override init() {
        super.init()
        requestAuthorization()
    }

    private func requestAuthorization() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { granted, error in
            self.isAuthorized = granted
            if let error = error {
                print("Notification authorization error: \(error)")
            }
        }

        UNUserNotificationCenter.current().delegate = self
    }

    /// Show a notification when connected to a server
    func notifyConnected(to server: String) {
        guard Preferences.shared.showNotifications else { return }

        let content = UNMutableNotificationContent()
        content.title = "Konflikt Connected"
        content.body = "Connected to \(server)"
        content.sound = .default

        let request = UNNotificationRequest(
            identifier: "connected-\(Date().timeIntervalSince1970)",
            content: content,
            trigger: nil
        )

        UNUserNotificationCenter.current().add(request)
    }

    /// Show a notification when disconnected
    func notifyDisconnected(reason: String? = nil) {
        guard Preferences.shared.showNotifications else { return }

        let content = UNMutableNotificationContent()
        content.title = "Konflikt Disconnected"
        content.body = reason ?? "Connection lost"
        content.sound = .default

        let request = UNNotificationRequest(
            identifier: "disconnected-\(Date().timeIntervalSince1970)",
            content: content,
            trigger: nil
        )

        UNUserNotificationCenter.current().add(request)
    }

    /// Show a notification for errors
    func notifyError(_ message: String) {
        guard Preferences.shared.showNotifications else { return }

        let content = UNMutableNotificationContent()
        content.title = "Konflikt Error"
        content.body = message
        content.sound = .defaultCritical

        let request = UNNotificationRequest(
            identifier: "error-\(Date().timeIntervalSince1970)",
            content: content,
            trigger: nil
        )

        UNUserNotificationCenter.current().add(request)
    }

    /// Show a notification that accessibility is required
    func notifyAccessibilityRequired() {
        let content = UNMutableNotificationContent()
        content.title = "Accessibility Permission Required"
        content.body = "Click to open System Preferences and enable Konflikt"
        content.sound = .default
        content.categoryIdentifier = "ACCESSIBILITY"

        let request = UNNotificationRequest(
            identifier: "accessibility",
            content: content,
            trigger: nil
        )

        UNUserNotificationCenter.current().add(request)
    }
}

// MARK: - UNUserNotificationCenterDelegate
extension NotificationManager: UNUserNotificationCenterDelegate {
    func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        willPresent notification: UNNotification,
        withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void
    ) {
        // Show notifications even when app is in foreground
        completionHandler([.banner, .sound])
    }

    func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        didReceive response: UNNotificationResponse,
        withCompletionHandler completionHandler: @escaping () -> Void
    ) {
        // Handle notification clicks
        if response.notification.request.content.categoryIdentifier == "ACCESSIBILITY" {
            AccessibilityHelper.requestAccessibilityPermissions()
        }
        completionHandler()
    }
}
