// Konflikt macOS Application
// Preferences Window Controller

import Cocoa

protocol PreferencesDelegate: AnyObject {
    func preferencesDidChange()
}

class PreferencesWindowController: NSWindowController {
    weak var delegate: PreferencesDelegate?
    weak var konflikt: Konflikt?

    private var lockCursorCheckbox: NSButton!
    private var edgeLeftCheckbox: NSButton!
    private var edgeRightCheckbox: NSButton!
    private var edgeTopCheckbox: NSButton!
    private var edgeBottomCheckbox: NSButton!

    convenience init(konflikt: Konflikt?) {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 320, height: 260),
            styleMask: [.titled, .closable],
            backing: .buffered,
            defer: false
        )
        window.title = "Preferences"
        window.center()

        self.init(window: window)
        self.konflikt = konflikt
        setupUI()
        loadCurrentSettings()
    }

    private func setupUI() {
        guard let window = window else { return }

        let contentView = NSView(frame: window.contentView!.bounds)
        contentView.autoresizingMask = [.width, .height]
        window.contentView = contentView

        var y: CGFloat = 220
        let leftMargin: CGFloat = 20
        let labelWidth: CGFloat = 280

        // Title
        let titleLabel = NSTextField(labelWithString: "Screen Edge Transitions")
        titleLabel.font = NSFont.boldSystemFont(ofSize: 13)
        titleLabel.frame = NSRect(x: leftMargin, y: y, width: labelWidth, height: 20)
        contentView.addSubview(titleLabel)
        y -= 30

        // Edge checkboxes
        edgeLeftCheckbox = NSButton(checkboxWithTitle: "Left edge", target: self, action: #selector(edgeSettingChanged))
        edgeLeftCheckbox.frame = NSRect(x: leftMargin + 10, y: y, width: 120, height: 20)
        contentView.addSubview(edgeLeftCheckbox)

        edgeRightCheckbox = NSButton(checkboxWithTitle: "Right edge", target: self, action: #selector(edgeSettingChanged))
        edgeRightCheckbox.frame = NSRect(x: leftMargin + 140, y: y, width: 120, height: 20)
        contentView.addSubview(edgeRightCheckbox)
        y -= 25

        edgeTopCheckbox = NSButton(checkboxWithTitle: "Top edge", target: self, action: #selector(edgeSettingChanged))
        edgeTopCheckbox.frame = NSRect(x: leftMargin + 10, y: y, width: 120, height: 20)
        contentView.addSubview(edgeTopCheckbox)

        edgeBottomCheckbox = NSButton(checkboxWithTitle: "Bottom edge", target: self, action: #selector(edgeSettingChanged))
        edgeBottomCheckbox.frame = NSRect(x: leftMargin + 140, y: y, width: 120, height: 20)
        contentView.addSubview(edgeBottomCheckbox)
        y -= 35

        // Separator
        let separator1 = NSBox()
        separator1.boxType = .separator
        separator1.frame = NSRect(x: leftMargin, y: y, width: 280, height: 1)
        contentView.addSubview(separator1)
        y -= 20

        // Cursor lock section
        let cursorTitle = NSTextField(labelWithString: "Cursor Lock")
        cursorTitle.font = NSFont.boldSystemFont(ofSize: 13)
        cursorTitle.frame = NSRect(x: leftMargin, y: y, width: labelWidth, height: 20)
        contentView.addSubview(cursorTitle)
        y -= 30

        lockCursorCheckbox = NSButton(checkboxWithTitle: "Lock cursor to this screen", target: self, action: #selector(lockCursorChanged))
        lockCursorCheckbox.frame = NSRect(x: leftMargin + 10, y: y, width: 200, height: 20)
        contentView.addSubview(lockCursorCheckbox)
        y -= 25

        let hotkeyLabel = NSTextField(labelWithString: "Press Scroll Lock to toggle cursor lock")
        hotkeyLabel.font = NSFont.systemFont(ofSize: 11)
        hotkeyLabel.textColor = .secondaryLabelColor
        hotkeyLabel.frame = NSRect(x: leftMargin + 10, y: y, width: 260, height: 16)
        contentView.addSubview(hotkeyLabel)
        y -= 35

        // Separator
        let separator2 = NSBox()
        separator2.boxType = .separator
        separator2.frame = NSRect(x: leftMargin, y: y, width: 280, height: 1)
        contentView.addSubview(separator2)
        y -= 30

        // Close button
        let closeButton = NSButton(title: "Close", target: self, action: #selector(closeWindow))
        closeButton.bezelStyle = .rounded
        closeButton.frame = NSRect(x: 220, y: 15, width: 80, height: 28)
        closeButton.keyEquivalent = "\u{1b}" // Escape key
        contentView.addSubview(closeButton)
    }

    private func loadCurrentSettings() {
        guard let konflikt = konflikt else { return }

        lockCursorCheckbox.state = konflikt.isLockCursorToScreen() ? .on : .off
        edgeLeftCheckbox.state = konflikt.edgeLeft() ? .on : .off
        edgeRightCheckbox.state = konflikt.edgeRight() ? .on : .off
        edgeTopCheckbox.state = konflikt.edgeTop() ? .on : .off
        edgeBottomCheckbox.state = konflikt.edgeBottom() ? .on : .off
    }

    @objc private func lockCursorChanged() {
        konflikt?.setLockCursorToScreen(lockCursorCheckbox.state == .on)
        delegate?.preferencesDidChange()
    }

    @objc private func edgeSettingChanged() {
        konflikt?.setEdgeLeft(
            edgeLeftCheckbox.state == .on,
            right: edgeRightCheckbox.state == .on,
            top: edgeTopCheckbox.state == .on,
            bottom: edgeBottomCheckbox.state == .on
        )
        delegate?.preferencesDidChange()
    }

    @objc private func closeWindow() {
        window?.close()
    }

    func showWindow() {
        loadCurrentSettings()
        window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}
