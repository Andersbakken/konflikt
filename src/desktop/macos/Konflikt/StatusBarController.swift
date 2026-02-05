import Cocoa

enum ConnectionStatus {
    case connected
    case connecting
    case disconnected
    case error
    case updating
}

protocol StatusBarControllerDelegate: AnyObject {
    func openConfiguration()
    func requestAccessibility()
    func toggleLaunchAtLogin(_ enabled: Bool)
    func isLaunchAtLoginEnabled() -> Bool
    func quitApplication()
    func restartBackend()
    func getConnectionStatus() -> (status: ConnectionStatus, serverName: String?)
    func isAccessibilityEnabled() -> Bool
}

class StatusBarController {
    private var statusItem: NSStatusItem?
    private var menu: NSMenu?
    private var statusMenuItem: NSMenuItem?
    private var accessibilityMenuItem: NSMenuItem?
    private var launchAtLoginMenuItem: NSMenuItem?

    weak var delegate: StatusBarControllerDelegate?

    private var currentStatus: ConnectionStatus = .disconnected
    private var currentMessage: String = "Disconnected"

    init() {
        setupStatusBar()
    }

    private func setupStatusBar() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)

        if let button = statusItem?.button {
            button.image = NSImage(systemSymbolName: "rectangle.connected.to.line.below", accessibilityDescription: "Konflikt")
            button.image?.isTemplate = true
        }

        setupMenu()
        updateStatusIcon()
    }

    private func setupMenu() {
        menu = NSMenu()

        // Status item (disabled, just for display)
        statusMenuItem = NSMenuItem(title: "Status: Disconnected", action: nil, keyEquivalent: "")
        statusMenuItem?.isEnabled = false
        menu?.addItem(statusMenuItem!)

        menu?.addItem(NSMenuItem.separator())

        // Open Configuration
        let configItem = NSMenuItem(title: "Open Configuration...", action: #selector(openConfigurationClicked), keyEquivalent: ",")
        configItem.target = self
        menu?.addItem(configItem)

        menu?.addItem(NSMenuItem.separator())

        // Accessibility status
        accessibilityMenuItem = NSMenuItem(title: "Accessibility: Checking...", action: #selector(accessibilityClicked), keyEquivalent: "")
        accessibilityMenuItem?.target = self
        menu?.addItem(accessibilityMenuItem!)

        menu?.addItem(NSMenuItem.separator())

        // Launch at Login
        launchAtLoginMenuItem = NSMenuItem(title: "Start at Login", action: #selector(launchAtLoginClicked), keyEquivalent: "")
        launchAtLoginMenuItem?.target = self
        menu?.addItem(launchAtLoginMenuItem!)

        // Restart
        let restartItem = NSMenuItem(title: "Restart Backend", action: #selector(restartClicked), keyEquivalent: "r")
        restartItem.target = self
        menu?.addItem(restartItem)

        menu?.addItem(NSMenuItem.separator())

        // Quit
        let quitItem = NSMenuItem(title: "Quit Konflikt", action: #selector(quitClicked), keyEquivalent: "q")
        quitItem.target = self
        menu?.addItem(quitItem)

        statusItem?.menu = menu

        // Update states
        updateAccessibilityStatus()
        updateLaunchAtLoginStatus()
    }

    func updateStatus(_ status: ConnectionStatus, message: String) {
        currentStatus = status
        currentMessage = message

        DispatchQueue.main.async {
            self.statusMenuItem?.title = "Status: \(message)"
            self.updateStatusIcon()
        }
    }

    private func updateStatusIcon() {
        guard let button = statusItem?.button else { return }

        let symbolName: String
        let color: NSColor

        switch currentStatus {
        case .connected:
            symbolName = "rectangle.connected.to.line.below"
            color = .systemGreen
        case .connecting:
            symbolName = "rectangle.connected.to.line.below"
            color = .systemYellow
        case .disconnected:
            symbolName = "rectangle.connected.to.line.below"
            color = .systemGray
        case .error:
            symbolName = "exclamationmark.triangle"
            color = .systemRed
        case .updating:
            symbolName = "arrow.triangle.2.circlepath"
            color = .systemBlue
        }

        if let image = NSImage(systemSymbolName: symbolName, accessibilityDescription: "Konflikt") {
            // Create colored version
            let config = NSImage.SymbolConfiguration(pointSize: 16, weight: .regular)
            let configuredImage = image.withSymbolConfiguration(config)

            // For template images, we can't easily colorize, so we use the template
            button.image = configuredImage
            button.image?.isTemplate = true

            // Add a badge or indicator for status
            // For simplicity, we'll use different symbols or add a small circle
            updateStatusBadge(color: color)
        }
    }

    private func updateStatusBadge(color: NSColor) {
        // For now, we'll just rely on the symbol change
        // A more sophisticated approach would overlay a colored dot
    }

    private func updateAccessibilityStatus() {
        let enabled = delegate?.isAccessibilityEnabled() ?? false
        DispatchQueue.main.async {
            if enabled {
                self.accessibilityMenuItem?.title = "Accessibility: Enabled"
                self.accessibilityMenuItem?.state = .on
            } else {
                self.accessibilityMenuItem?.title = "Accessibility: Click to Enable"
                self.accessibilityMenuItem?.state = .off
            }
        }
    }

    private func updateLaunchAtLoginStatus() {
        let enabled = delegate?.isLaunchAtLoginEnabled() ?? false
        DispatchQueue.main.async {
            self.launchAtLoginMenuItem?.state = enabled ? .on : .off
        }
    }

    func showAccessibilityAlert() {
        let alert = NSAlert()
        alert.messageText = "Accessibility Permission Required"
        alert.informativeText = "Konflikt needs Accessibility permission to control your mouse and keyboard across screens.\n\nClick 'Open System Preferences' to grant access, then check the box next to Konflikt."
        alert.alertStyle = .warning
        alert.addButton(withTitle: "Open System Preferences")
        alert.addButton(withTitle: "Later")

        let response = alert.runModal()
        if response == .alertFirstButtonReturn {
            delegate?.requestAccessibility()
        }
    }

    // MARK: - Actions

    @objc private func openConfigurationClicked() {
        delegate?.openConfiguration()
    }

    @objc private func accessibilityClicked() {
        if delegate?.isAccessibilityEnabled() == false {
            delegate?.requestAccessibility()
        }
        // Refresh status after a delay
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            self.updateAccessibilityStatus()
        }
    }

    @objc private func launchAtLoginClicked() {
        let currentlyEnabled = delegate?.isLaunchAtLoginEnabled() ?? false
        delegate?.toggleLaunchAtLogin(!currentlyEnabled)
        updateLaunchAtLoginStatus()
    }

    @objc private func restartClicked() {
        delegate?.restartBackend()
    }

    @objc private func quitClicked() {
        delegate?.quitApplication()
    }
}
