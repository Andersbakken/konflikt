import Cocoa
import ServiceManagement

class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusBarController: StatusBarController?
    private var processManager: ProcessManager?
    private var preferencesWindow: PreferencesWindow?
    private var configPort: Int = 3000

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Hide dock icon - we're a menu bar only app
        NSApp.setActivationPolicy(.accessory)

        // Initialize process manager
        processManager = ProcessManager()
        processManager?.delegate = self

        // Initialize status bar
        statusBarController = StatusBarController()
        statusBarController?.delegate = self

        // Check accessibility permissions on launch
        if !AccessibilityHelper.isAccessibilityEnabled() {
            // Show a brief delay then prompt
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                self.statusBarController?.showAccessibilityAlert()
            }
        }

        // Show preferences on first launch
        if Preferences.shared.isFirstLaunch {
            showPreferences()
        }

        // Start the backend process if auto-connect is enabled
        if Preferences.shared.autoConnect {
            startBackend()
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        processManager?.stopProcess()
    }

    private func startBackend() {
        guard let processManager = processManager else { return }

        // Find the Node.js backend path relative to the app bundle
        let backendPath = findBackendPath()

        if let path = backendPath {
            let prefs = Preferences.shared
            processManager.startProcess(
                backendPath: path,
                role: prefs.role,
                serverAddress: prefs.serverAddress,
                port: prefs.serverPort > 0 ? prefs.serverPort : nil,
                verbose: prefs.verboseLogging,
                logFilePath: prefs.logFilePath.isEmpty ? nil : prefs.logFilePath
            )
        } else {
            print("Could not find backend path")
            statusBarController?.updateStatus(.error, message: "Backend not found")
        }
    }

    private func showPreferences() {
        if preferencesWindow == nil {
            preferencesWindow = PreferencesWindow()
            preferencesWindow?.preferencesDelegate = self
        }
        preferencesWindow?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func findBackendPath() -> String? {
        // Check in the app bundle's Resources folder first
        if let bundlePath = Bundle.main.resourcePath {
            let bundledBackend = (bundlePath as NSString).appendingPathComponent("backend/dist/app/index.js")
            if FileManager.default.fileExists(atPath: bundledBackend) {
                return bundledBackend
            }
        }

        // Check for development path (relative to app location)
        let appPath = Bundle.main.bundlePath
        let devPath = ((appPath as NSString).deletingLastPathComponent as NSString).appendingPathComponent("dist/app/index.js")
        if FileManager.default.fileExists(atPath: devPath) {
            return devPath
        }

        // Check common development locations
        let homeDir = FileManager.default.homeDirectoryForCurrentUser.path
        let commonPaths = [
            "\(homeDir)/dev/konflikt/dist/app/index.js",
            "/usr/local/share/konflikt/dist/app/index.js",
            "/opt/konflikt/dist/app/index.js"
        ]

        for path in commonPaths {
            if FileManager.default.fileExists(atPath: path) {
                return path
            }
        }

        return nil
    }

}

// MARK: - PreferencesWindowDelegate
extension AppDelegate: PreferencesWindowDelegate {
    func preferencesDidChange() {
        // Restart backend with new settings
        processManager?.stopProcess()
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            self.startBackend()
        }
    }
}

// MARK: - StatusBarControllerDelegate
extension AppDelegate: StatusBarControllerDelegate {
    func openConfiguration() {
        let url = URL(string: "http://localhost:\(configPort)/ui")!
        NSWorkspace.shared.open(url)
    }

    func openPreferences() {
        showPreferences()
    }

    func requestAccessibility() {
        _ = AccessibilityHelper.requestAccessibilityPermissions()
    }

    func toggleLaunchAtLogin(_ enabled: Bool) {
        LaunchAtLoginHelper.setLaunchAtLogin(enabled: enabled)
    }

    func isLaunchAtLoginEnabled() -> Bool {
        return LaunchAtLoginHelper.isLaunchAtLoginEnabled()
    }

    func quitApplication() {
        processManager?.stopProcess()
        NSApp.terminate(nil)
    }

    func restartBackend() {
        processManager?.stopProcess()
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            self.startBackend()
        }
    }

    func getConnectionStatus() -> (status: ConnectionStatus, serverName: String?) {
        return processManager?.connectionStatus ?? (.disconnected, nil)
    }

    func isAccessibilityEnabled() -> Bool {
        return AccessibilityHelper.isAccessibilityEnabled()
    }
}

// MARK: - ProcessManagerDelegate
extension AppDelegate: ProcessManagerDelegate {
    func processDidStart() {
        statusBarController?.updateStatus(.connecting, message: "Starting...")
    }

    func processDidConnect(to server: String) {
        statusBarController?.updateStatus(.connected, message: "Connected to \(server)")
        NotificationManager.shared.notifyConnected(to: server)
    }

    func processDidDisconnect() {
        statusBarController?.updateStatus(.disconnected, message: "Disconnected")
        NotificationManager.shared.notifyDisconnected()
    }

    func processDidFail(with error: String) {
        statusBarController?.updateStatus(.error, message: error)
        NotificationManager.shared.notifyError(error)
    }

    func processDidTerminate(exitCode: Int32) {
        if exitCode == 42 {
            // Update required - pull and rebuild
            statusBarController?.updateStatus(.updating, message: "Updating...")
            performUpdate()
        } else if exitCode != 0 {
            statusBarController?.updateStatus(.error, message: "Exited with code \(exitCode)")
            // Auto-restart after delay
            DispatchQueue.main.asyncAfter(deadline: .now() + 3.0) {
                self.startBackend()
            }
        }
    }

    func processPortChanged(to port: Int) {
        configPort = port
    }

    private func performUpdate() {
        // This would run git pull and npm rebuild
        // For now, just restart
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            self.startBackend()
        }
    }
}
