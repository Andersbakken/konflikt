// Konflikt macOS Application
// Status Bar Controller

import Cocoa

class StatusBarController {
    private let statusItem: NSStatusItem
    private let menu: NSMenu
    private var statusMenuItem: NSMenuItem?
    private var clientsMenuItem: NSMenuItem?
    private var clientsSubmenu: NSMenu?
    private var lockCursorMenuItem: NSMenuItem?
    private var preferencesWindowController: PreferencesWindowController?
    private var updateTimer: Timer?
    weak var konflikt: Konflikt?

    init() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        menu = NSMenu()

        setupStatusItem()
        setupMenu()
        startUpdateTimer()
    }

    private func setupStatusItem() {
        if let button = statusItem.button {
            button.image = NSImage(systemSymbolName: "rectangle.connected.to.line.below", accessibilityDescription: "Konflikt")
            button.image?.isTemplate = true
        }
        statusItem.menu = menu
    }

    private func setupMenu() {
        // Status item
        statusMenuItem = NSMenuItem(title: "Starting...", action: nil, keyEquivalent: "")
        statusMenuItem?.isEnabled = false
        menu.addItem(statusMenuItem!)

        // Clients submenu
        clientsMenuItem = NSMenuItem(title: "Clients (0)", action: nil, keyEquivalent: "")
        clientsSubmenu = NSMenu()
        clientsMenuItem?.submenu = clientsSubmenu
        menu.addItem(clientsMenuItem!)

        menu.addItem(NSMenuItem.separator())

        // Open UI
        let openUIItem = NSMenuItem(title: "Open Web UI", action: #selector(openWebUI), keyEquivalent: "o")
        openUIItem.target = self
        menu.addItem(openUIItem)

        // Lock cursor toggle
        lockCursorMenuItem = NSMenuItem(title: "Lock Cursor to Screen", action: #selector(toggleLockCursor), keyEquivalent: "l")
        lockCursorMenuItem?.target = self
        menu.addItem(lockCursorMenuItem!)

        // Preferences
        let prefsItem = NSMenuItem(title: "Preferences...", action: #selector(showPreferences), keyEquivalent: ",")
        prefsItem.target = self
        menu.addItem(prefsItem)

        menu.addItem(NSMenuItem.separator())

        // About
        let aboutItem = NSMenuItem(title: "About Konflikt", action: #selector(showAbout), keyEquivalent: "")
        aboutItem.target = self
        menu.addItem(aboutItem)

        // Quit
        let quitItem = NSMenuItem(title: "Quit", action: #selector(quit), keyEquivalent: "q")
        quitItem.target = self
        menu.addItem(quitItem)
    }

    private func startUpdateTimer() {
        updateTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            self?.updateClientsMenu()
        }
    }

    private func updateClientsMenu() {
        guard let konflikt = konflikt else { return }

        let count = konflikt.clientCount()
        let names = konflikt.connectedClientNames()
        let isLocked = konflikt.isLockCursorToScreen()

        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }

            self.clientsMenuItem?.title = "Clients (\(count))"
            self.clientsSubmenu?.removeAllItems()

            if names.isEmpty {
                let noClientsItem = NSMenuItem(title: "No clients connected", action: nil, keyEquivalent: "")
                noClientsItem.isEnabled = false
                self.clientsSubmenu?.addItem(noClientsItem)
            } else {
                for name in names {
                    let item = NSMenuItem(title: name, action: nil, keyEquivalent: "")
                    item.isEnabled = false
                    self.clientsSubmenu?.addItem(item)
                }
            }

            // Update lock cursor state
            self.lockCursorMenuItem?.state = isLocked ? .on : .off
        }
    }

    func updateStatus(_ status: String) {
        DispatchQueue.main.async { [weak self] in
            self?.statusMenuItem?.title = status
        }
    }

    @objc private func openWebUI() {
        // Default port
        let port = 3000
        if let url = URL(string: "http://localhost:\(port)/ui/") {
            NSWorkspace.shared.open(url)
        }
    }

    @objc private func toggleLockCursor() {
        guard let konflikt = konflikt else { return }
        let newState = !konflikt.isLockCursorToScreen()
        konflikt.setLockCursorToScreen(newState)
        lockCursorMenuItem?.state = newState ? .on : .off
    }

    @objc private func showPreferences() {
        if preferencesWindowController == nil {
            preferencesWindowController = PreferencesWindowController(konflikt: konflikt)
        }
        preferencesWindowController?.showWindow()
    }

    @objc private func showAbout() {
        let alert = NSAlert()
        alert.messageText = "Konflikt"
        alert.informativeText = "Version 2.0.0\n\nSoftware KVM Switch\nShare keyboard and mouse between computers.\n\nhttps://github.com/Andersbakken/konflikt"
        alert.alertStyle = .informational
        alert.addButton(withTitle: "OK")
        alert.runModal()
    }

    @objc private func quit() {
        NSApplication.shared.terminate(nil)
    }
}
