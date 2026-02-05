import Cocoa
import ApplicationServices

class AccessibilityHelper {
    /// Check if accessibility permissions are currently granted
    static func isAccessibilityEnabled() -> Bool {
        return AXIsProcessTrusted()
    }

    /// Request accessibility permissions, showing the system prompt if not already granted
    /// Returns true if permissions are already granted, false if user needs to grant them
    @discardableResult
    static func requestAccessibilityPermissions() -> Bool {
        let options = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        return AXIsProcessTrustedWithOptions(options)
    }

    /// Open System Preferences directly to the Accessibility pane
    static func openAccessibilityPreferences() {
        let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!
        NSWorkspace.shared.open(url)
    }

    /// Check and prompt for accessibility if needed, with a custom message
    static func ensureAccessibility(showAlert: Bool = true, completion: @escaping (Bool) -> Void) {
        if isAccessibilityEnabled() {
            completion(true)
            return
        }

        if showAlert {
            DispatchQueue.main.async {
                let alert = NSAlert()
                alert.messageText = "Accessibility Access Required"
                alert.informativeText = """
                    Konflikt needs accessibility permissions to:
                    • Control your mouse cursor across screens
                    • Send keyboard input to remote machines
                    • Function as a KVM switch

                    After clicking "Open System Preferences", find Konflikt in the list and enable it.
                    """
                alert.alertStyle = .warning
                alert.addButton(withTitle: "Open System Preferences")
                alert.addButton(withTitle: "Cancel")

                let response = alert.runModal()
                if response == .alertFirstButtonReturn {
                    requestAccessibilityPermissions()
                    // Poll for permission grant
                    pollForAccessibility(completion: completion)
                } else {
                    completion(false)
                }
            }
        } else {
            requestAccessibilityPermissions()
            pollForAccessibility(completion: completion)
        }
    }

    /// Poll for accessibility permission to be granted (user might take time in System Preferences)
    private static func pollForAccessibility(timeout: TimeInterval = 60, completion: @escaping (Bool) -> Void) {
        let startTime = Date()

        func check() {
            if isAccessibilityEnabled() {
                completion(true)
                return
            }

            if Date().timeIntervalSince(startTime) > timeout {
                completion(false)
                return
            }

            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                check()
            }
        }

        check()
    }
}
