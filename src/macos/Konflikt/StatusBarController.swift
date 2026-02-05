// Konflikt macOS Application
// Status Bar Controller

import Cocoa

class StatusBarController {
    private let statusItem: NSStatusItem
    private let menu: NSMenu
    private var statusMenuItem: NSMenuItem?

    init() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        menu = NSMenu()

        setupStatusItem()
        setupMenu()
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

        menu.addItem(NSMenuItem.separator())

        // Open UI
        let openUIItem = NSMenuItem(title: "Open Web UI", action: #selector(openWebUI), keyEquivalent: "o")
        openUIItem.target = self
        menu.addItem(openUIItem)

        menu.addItem(NSMenuItem.separator())

        // Quit
        let quitItem = NSMenuItem(title: "Quit", action: #selector(quit), keyEquivalent: "q")
        quitItem.target = self
        menu.addItem(quitItem)
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

    @objc private func quit() {
        NSApplication.shared.terminate(nil)
    }
}
