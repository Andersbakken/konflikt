#!/bin/bash

# Generate app icon for Konflikt
# This creates a simple icon using Core Graphics

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ICONSET_DIR="$SCRIPT_DIR/Konflikt/AppIcon.iconset"
ICON_DIR="$SCRIPT_DIR/Konflikt"

# Create iconset directory
mkdir -p "$ICONSET_DIR"

# Generate icon using a Swift script
cat > /tmp/generate_icon.swift << 'SWIFT'
import Cocoa

func createIcon(size: Int) -> NSImage {
    let image = NSImage(size: NSSize(width: size, height: size))

    image.lockFocus()

    // Background gradient
    let gradient = NSGradient(colors: [
        NSColor(red: 0.2, green: 0.6, blue: 0.9, alpha: 1.0),
        NSColor(red: 0.1, green: 0.4, blue: 0.8, alpha: 1.0)
    ])

    let rect = NSRect(x: 0, y: 0, width: size, height: size)
    let cornerRadius = CGFloat(size) * 0.2
    let path = NSBezierPath(roundedRect: rect, xRadius: cornerRadius, yRadius: cornerRadius)

    gradient?.draw(in: path, angle: -90)

    // Draw "K" letter
    let fontSize = CGFloat(size) * 0.6
    let font = NSFont.systemFont(ofSize: fontSize, weight: .bold)
    let attributes: [NSAttributedString.Key: Any] = [
        .font: font,
        .foregroundColor: NSColor.white
    ]

    let text = "K"
    let textSize = text.size(withAttributes: attributes)
    let textRect = NSRect(
        x: (CGFloat(size) - textSize.width) / 2,
        y: (CGFloat(size) - textSize.height) / 2,
        width: textSize.width,
        height: textSize.height
    )

    text.draw(in: textRect, withAttributes: attributes)

    // Draw connection lines
    NSColor.white.withAlphaComponent(0.6).setStroke()

    let lineWidth = CGFloat(size) * 0.03
    let line1 = NSBezierPath()
    line1.lineWidth = lineWidth
    line1.move(to: NSPoint(x: CGFloat(size) * 0.15, y: CGFloat(size) * 0.3))
    line1.line(to: NSPoint(x: CGFloat(size) * 0.35, y: CGFloat(size) * 0.5))
    line1.stroke()

    let line2 = NSBezierPath()
    line2.lineWidth = lineWidth
    line2.move(to: NSPoint(x: CGFloat(size) * 0.65, y: CGFloat(size) * 0.5))
    line2.line(to: NSPoint(x: CGFloat(size) * 0.85, y: CGFloat(size) * 0.7))
    line2.stroke()

    image.unlockFocus()

    return image
}

func saveIcon(_ image: NSImage, to path: String) {
    guard let tiffData = image.tiffRepresentation,
          let bitmap = NSBitmapImageRep(data: tiffData),
          let pngData = bitmap.representation(using: .png, properties: [:]) else {
        print("Failed to create PNG data")
        return
    }

    do {
        try pngData.write(to: URL(fileURLWithPath: path))
        print("Created: \(path)")
    } catch {
        print("Failed to write \(path): \(error)")
    }
}

let sizes = [16, 32, 64, 128, 256, 512, 1024]
let iconsetPath = CommandLine.arguments[1]

for size in sizes {
    let image = createIcon(size: size)
    saveIcon(image, to: "\(iconsetPath)/icon_\(size)x\(size).png")

    // Also create @2x versions for Retina
    if size <= 512 {
        let image2x = createIcon(size: size * 2)
        saveIcon(image2x, to: "\(iconsetPath)/icon_\(size)x\(size)@2x.png")
    }
}

print("Icon generation complete!")
SWIFT

# Compile and run the icon generator
echo "Generating app icon..."
swiftc -o /tmp/generate_icon /tmp/generate_icon.swift -framework Cocoa 2>/dev/null

if [ -f /tmp/generate_icon ]; then
    /tmp/generate_icon "$ICONSET_DIR"

    # Convert iconset to icns
    if [ -d "$ICONSET_DIR" ]; then
        iconutil -c icns "$ICONSET_DIR" -o "$ICON_DIR/AppIcon.icns"
        echo "Created: $ICON_DIR/AppIcon.icns"
    fi

    rm -f /tmp/generate_icon /tmp/generate_icon.swift
else
    echo "Failed to compile icon generator (this is normal on non-macOS systems)"
    echo "The icon will be generated when you run build.sh on macOS"
fi
