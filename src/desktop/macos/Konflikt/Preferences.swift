import Foundation

/// Manages persistent preferences for the Konflikt app
class Preferences {
    static let shared = Preferences()

    private let defaults = UserDefaults.standard

    // MARK: - Keys
    private enum Keys {
        static let serverAddress = "serverAddress"
        static let serverPort = "serverPort"
        static let role = "role"
        static let autoConnect = "autoConnect"
        static let launchAtLogin = "launchAtLogin"
        static let showNotifications = "showNotifications"
        static let verboseLogging = "verboseLogging"
    }

    // MARK: - Server Settings

    /// The server address to connect to (for client role)
    var serverAddress: String {
        get { defaults.string(forKey: Keys.serverAddress) ?? "" }
        set { defaults.set(newValue, forKey: Keys.serverAddress) }
    }

    /// The port to use (0 = auto-select)
    var serverPort: Int {
        get { defaults.integer(forKey: Keys.serverPort) }
        set { defaults.set(newValue, forKey: Keys.serverPort) }
    }

    /// The role: "client" or "server"
    var role: String {
        get { defaults.string(forKey: Keys.role) ?? "client" }
        set { defaults.set(newValue, forKey: Keys.role) }
    }

    // MARK: - Behavior Settings

    /// Whether to automatically connect on launch
    var autoConnect: Bool {
        get { defaults.bool(forKey: Keys.autoConnect) }
        set { defaults.set(newValue, forKey: Keys.autoConnect) }
    }

    /// Whether to launch at login
    var launchAtLogin: Bool {
        get { defaults.bool(forKey: Keys.launchAtLogin) }
        set {
            defaults.set(newValue, forKey: Keys.launchAtLogin)
            LaunchAtLoginHelper.setLaunchAtLogin(enabled: newValue)
        }
    }

    /// Whether to show system notifications for events
    var showNotifications: Bool {
        get { defaults.object(forKey: Keys.showNotifications) == nil ? true : defaults.bool(forKey: Keys.showNotifications) }
        set { defaults.set(newValue, forKey: Keys.showNotifications) }
    }

    /// Whether to enable verbose logging
    var verboseLogging: Bool {
        get { defaults.bool(forKey: Keys.verboseLogging) }
        set { defaults.set(newValue, forKey: Keys.verboseLogging) }
    }

    // MARK: - Computed Properties

    /// Get command line arguments based on current preferences
    var commandLineArguments: [String] {
        var args: [String] = []

        args.append("--role=\(role)")

        if !serverAddress.isEmpty && role == "client" {
            args.append("--server=\(serverAddress)")
        }

        if serverPort > 0 {
            args.append("--port=\(serverPort)")
        }

        if verboseLogging {
            args.append("-vv")
        }

        return args
    }

    // MARK: - Initialization

    private init() {
        // Register default values
        defaults.register(defaults: [
            Keys.role: "client",
            Keys.autoConnect: true,
            Keys.showNotifications: true,
            Keys.verboseLogging: false,
            Keys.serverPort: 0
        ])
    }

    // MARK: - Methods

    /// Reset all preferences to defaults
    func resetToDefaults() {
        let domain = Bundle.main.bundleIdentifier!
        defaults.removePersistentDomain(forName: domain)
        defaults.synchronize()
    }

    /// Check if this is the first launch
    var isFirstLaunch: Bool {
        let key = "hasLaunchedBefore"
        if defaults.bool(forKey: key) {
            return false
        }
        defaults.set(true, forKey: key)
        return true
    }
}
