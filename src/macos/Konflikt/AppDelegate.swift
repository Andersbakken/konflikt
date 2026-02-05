// Konflikt macOS Application
// App Delegate

import Cocoa

class AppDelegate: NSObject, NSApplicationDelegate, KonfliktDelegate {
    private var statusBarController: StatusBarController?
    private var konflikt: Konflikt?
    private var backgroundThread: Thread?

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Create status bar controller
        statusBarController = StatusBarController()

        // Configure Konflikt
        let config = KonfliktConfig.default()
        config.role = .server
        config.instanceName = Host.current().localizedName ?? "Mac"

        // Find UI path
        if let bundlePath = Bundle.main.resourcePath {
            let uiPath = (bundlePath as NSString).appendingPathComponent("ui")
            if FileManager.default.fileExists(atPath: uiPath) {
                config.uiPath = uiPath
            }
        }

        // Create and configure Konflikt instance
        konflikt = Konflikt(config: config)
        konflikt?.delegate = self

        // Initialize
        guard konflikt?.initialize() == true else {
            showAlert(title: "Initialization Failed", message: "Failed to initialize Konflikt")
            NSApplication.shared.terminate(nil)
            return
        }

        // Pass konflikt instance to status bar controller for preferences
        statusBarController?.konflikt = konflikt

        // Start on background thread
        backgroundThread = Thread { [weak self] in
            self?.konflikt?.run()
        }
        backgroundThread?.start()

        // Update status bar
        if let port = konflikt?.httpPort {
            statusBarController?.updateStatus("Running on port \(port)")
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        konflikt?.quit()
        konflikt?.stop()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return false // Keep running as menu bar app
    }

    // MARK: - KonfliktDelegate

    func konflikt(_ konflikt: Any, didUpdate status: KonfliktConnectionStatus, message: String) {
        var statusText: String
        switch status {
        case .disconnected:
            statusText = "Disconnected"
        case .connecting:
            statusText = "Connecting..."
        case .connected:
            statusText = "Connected"
        case .error:
            statusText = "Error: \(message)"
        @unknown default:
            statusText = "Unknown"
        }
        statusBarController?.updateStatus(statusText)
    }

    func konflikt(_ konflikt: Any, didLog level: String, message: String) {
        print("[\(level)] \(message)")
    }

    // MARK: - Helpers

    private func showAlert(title: String, message: String) {
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = message
        alert.alertStyle = .critical
        alert.runModal()
    }
}
