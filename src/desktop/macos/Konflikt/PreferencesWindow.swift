import Cocoa

class PreferencesWindow: NSWindow {
    private var serverAddressField: NSTextField!
    private var portField: NSTextField!
    private var rolePopup: NSPopUpButton!
    private var autoConnectCheckbox: NSButton!
    private var launchAtLoginCheckbox: NSButton!
    private var notificationsCheckbox: NSButton!
    private var verboseCheckbox: NSButton!

    weak var preferencesDelegate: PreferencesWindowDelegate?

    init() {
        super.init(
            contentRect: NSRect(x: 0, y: 0, width: 400, height: 320),
            styleMask: [.titled, .closable],
            backing: .buffered,
            defer: false
        )

        self.title = "Konflikt Preferences"
        self.center()
        self.isReleasedWhenClosed = false

        setupUI()
        loadPreferences()
    }

    private func setupUI() {
        let contentView = NSView(frame: self.contentRect(forFrameRect: self.frame))
        self.contentView = contentView

        var yOffset: CGFloat = 280

        // Title
        let titleLabel = NSTextField(labelWithString: "Konflikt Settings")
        titleLabel.font = NSFont.boldSystemFont(ofSize: 16)
        titleLabel.frame = NSRect(x: 20, y: yOffset, width: 360, height: 24)
        contentView.addSubview(titleLabel)

        yOffset -= 40

        // Role selection
        let roleLabel = NSTextField(labelWithString: "Role:")
        roleLabel.frame = NSRect(x: 20, y: yOffset, width: 100, height: 22)
        contentView.addSubview(roleLabel)

        rolePopup = NSPopUpButton(frame: NSRect(x: 130, y: yOffset - 2, width: 150, height: 26))
        rolePopup.addItems(withTitles: ["Client", "Server"])
        rolePopup.target = self
        rolePopup.action = #selector(roleChanged(_:))
        contentView.addSubview(rolePopup)

        yOffset -= 35

        // Server address
        let serverLabel = NSTextField(labelWithString: "Server Address:")
        serverLabel.frame = NSRect(x: 20, y: yOffset, width: 100, height: 22)
        contentView.addSubview(serverLabel)

        serverAddressField = NSTextField(frame: NSRect(x: 130, y: yOffset - 2, width: 220, height: 24))
        serverAddressField.placeholderString = "hostname or IP"
        contentView.addSubview(serverAddressField)

        yOffset -= 35

        // Port
        let portLabel = NSTextField(labelWithString: "Port:")
        portLabel.frame = NSRect(x: 20, y: yOffset, width: 100, height: 22)
        contentView.addSubview(portLabel)

        portField = NSTextField(frame: NSRect(x: 130, y: yOffset - 2, width: 80, height: 24))
        portField.placeholderString = "Auto"
        contentView.addSubview(portField)

        let portHint = NSTextField(labelWithString: "(0 = auto)")
        portHint.font = NSFont.systemFont(ofSize: 11)
        portHint.textColor = .secondaryLabelColor
        portHint.frame = NSRect(x: 220, y: yOffset, width: 100, height: 20)
        contentView.addSubview(portHint)

        yOffset -= 40

        // Separator
        let separator = NSBox(frame: NSRect(x: 20, y: yOffset, width: 360, height: 1))
        separator.boxType = .separator
        contentView.addSubview(separator)

        yOffset -= 25

        // Auto-connect
        autoConnectCheckbox = NSButton(checkboxWithTitle: "Connect automatically on launch", target: nil, action: nil)
        autoConnectCheckbox.frame = NSRect(x: 20, y: yOffset, width: 300, height: 22)
        contentView.addSubview(autoConnectCheckbox)

        yOffset -= 25

        // Launch at login
        launchAtLoginCheckbox = NSButton(checkboxWithTitle: "Start Konflikt at login", target: nil, action: nil)
        launchAtLoginCheckbox.frame = NSRect(x: 20, y: yOffset, width: 300, height: 22)
        contentView.addSubview(launchAtLoginCheckbox)

        yOffset -= 25

        // Notifications
        notificationsCheckbox = NSButton(checkboxWithTitle: "Show notifications", target: nil, action: nil)
        notificationsCheckbox.frame = NSRect(x: 20, y: yOffset, width: 300, height: 22)
        contentView.addSubview(notificationsCheckbox)

        yOffset -= 25

        // Verbose logging
        verboseCheckbox = NSButton(checkboxWithTitle: "Verbose logging", target: nil, action: nil)
        verboseCheckbox.frame = NSRect(x: 20, y: yOffset, width: 300, height: 22)
        contentView.addSubview(verboseCheckbox)

        yOffset -= 40

        // Buttons
        let cancelButton = NSButton(title: "Cancel", target: self, action: #selector(cancelClicked(_:)))
        cancelButton.frame = NSRect(x: 200, y: 15, width: 80, height: 32)
        cancelButton.bezelStyle = .rounded
        cancelButton.keyEquivalent = "\u{1b}"  // Escape
        contentView.addSubview(cancelButton)

        let saveButton = NSButton(title: "Save", target: self, action: #selector(saveClicked(_:)))
        saveButton.frame = NSRect(x: 290, y: 15, width: 80, height: 32)
        saveButton.bezelStyle = .rounded
        saveButton.keyEquivalent = "\r"  // Enter
        contentView.addSubview(saveButton)
    }

    private func loadPreferences() {
        let prefs = Preferences.shared

        serverAddressField.stringValue = prefs.serverAddress
        portField.stringValue = prefs.serverPort > 0 ? String(prefs.serverPort) : ""
        rolePopup.selectItem(withTitle: prefs.role == "server" ? "Server" : "Client")
        autoConnectCheckbox.state = prefs.autoConnect ? .on : .off
        launchAtLoginCheckbox.state = prefs.launchAtLogin ? .on : .off
        notificationsCheckbox.state = prefs.showNotifications ? .on : .off
        verboseCheckbox.state = prefs.verboseLogging ? .on : .off

        updateUIForRole()
    }

    private func updateUIForRole() {
        let isClient = rolePopup.titleOfSelectedItem == "Client"
        serverAddressField.isEnabled = isClient
    }

    @objc private func roleChanged(_ sender: NSPopUpButton) {
        updateUIForRole()
    }

    @objc private func cancelClicked(_ sender: NSButton) {
        self.close()
    }

    @objc private func saveClicked(_ sender: NSButton) {
        let prefs = Preferences.shared

        prefs.serverAddress = serverAddressField.stringValue
        prefs.serverPort = Int(portField.stringValue) ?? 0
        prefs.role = rolePopup.titleOfSelectedItem == "Server" ? "server" : "client"
        prefs.autoConnect = autoConnectCheckbox.state == .on
        prefs.launchAtLogin = launchAtLoginCheckbox.state == .on
        prefs.showNotifications = notificationsCheckbox.state == .on
        prefs.verboseLogging = verboseCheckbox.state == .on

        preferencesDelegate?.preferencesDidChange()
        self.close()
    }
}

protocol PreferencesWindowDelegate: AnyObject {
    func preferencesDidChange()
}
