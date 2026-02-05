import Foundation
import ServiceManagement

class LaunchAtLoginHelper {
    private static let launcherBundleId = "com.konflikt.LauncherHelper"

    /// Check if the app is set to launch at login
    static func isLaunchAtLoginEnabled() -> Bool {
        // For modern macOS (13+), use SMAppService
        if #available(macOS 13.0, *) {
            return SMAppService.mainApp.status == .enabled
        } else {
            // For older macOS, use the deprecated but functional SMLoginItemSetEnabled
            // We can't easily check the status, so we'll use UserDefaults to track it
            return UserDefaults.standard.bool(forKey: "launchAtLogin")
        }
    }

    /// Enable or disable launch at login
    static func setLaunchAtLogin(enabled: Bool) {
        if #available(macOS 13.0, *) {
            do {
                if enabled {
                    try SMAppService.mainApp.register()
                } else {
                    try SMAppService.mainApp.unregister()
                }
            } catch {
                print("Failed to set launch at login: \(error)")
            }
        } else {
            // For older macOS, use SMLoginItemSetEnabled
            // Note: This requires a helper app in the bundle, which is more complex
            // For simplicity, we'll use a LaunchAgent approach instead
            setLaunchAtLoginViaLaunchAgent(enabled: enabled)
        }

        // Save preference
        UserDefaults.standard.set(enabled, forKey: "launchAtLogin")
    }

    /// Use LaunchAgent for older macOS versions
    private static func setLaunchAtLoginViaLaunchAgent(enabled: Bool) {
        let launchAgentsDir = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/LaunchAgents")

        let plistPath = launchAgentsDir.appendingPathComponent("com.konflikt.app.plist")

        if enabled {
            // Create LaunchAgent plist
            let appPath = Bundle.main.bundlePath

            let plistContent: [String: Any] = [
                "Label": "com.konflikt.app",
                "ProgramArguments": ["/usr/bin/open", "-a", appPath],
                "RunAtLoad": true,
                "KeepAlive": false
            ]

            // Ensure directory exists
            try? FileManager.default.createDirectory(at: launchAgentsDir, withIntermediateDirectories: true)

            // Write plist
            let plistData = try? PropertyListSerialization.data(
                fromPropertyList: plistContent,
                format: .xml,
                options: 0
            )

            if let data = plistData {
                try? data.write(to: plistPath)
            }
        } else {
            // Remove LaunchAgent plist
            try? FileManager.default.removeItem(at: plistPath)
        }
    }
}
