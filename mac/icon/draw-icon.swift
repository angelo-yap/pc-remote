// draw-icon.swift: draws the Launch Windows app icon (four-pane window) as a 1024px PNG.
// Usage: swift draw-icon.swift out.png
import AppKit

let size: CGFloat = 1024
let out = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "icon.png"

let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(size), pixelsHigh: Int(size),
                           bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                           colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)

// Background: macOS-style rounded square with a dark gradient (inset like Apple's icon grid).
let inset: CGFloat = 100
let bgRect = NSRect(x: inset, y: inset, width: size - 2 * inset, height: size - 2 * inset)
let bg = NSBezierPath(roundedRect: bgRect, xRadius: 185, yRadius: 185)
NSGradient(starting: NSColor(calibratedRed: 0.10, green: 0.12, blue: 0.18, alpha: 1),
           ending: NSColor(calibratedRed: 0.03, green: 0.04, blue: 0.07, alpha: 1))!
  .draw(in: bg, angle: -90)

// Four panes in a 2x2 grid.
let gridSize: CGFloat = 440
let gap: CGFloat = 26
let pane = (gridSize - gap) / 2
let origin = (size - gridSize) / 2
let paneGradient = NSGradient(starting: NSColor(calibratedRed: 0.20, green: 0.72, blue: 1.00, alpha: 1),
                              ending: NSColor(calibratedRed: 0.00, green: 0.47, blue: 0.85, alpha: 1))!
for row in 0..<2 {
  for col in 0..<2 {
    let r = NSRect(x: origin + CGFloat(col) * (pane + gap),
                   y: origin + CGFloat(row) * (pane + gap),
                   width: pane, height: pane)
    paneGradient.draw(in: NSBezierPath(roundedRect: r, xRadius: 14, yRadius: 14), angle: -90)
  }
}

NSGraphicsContext.restoreGraphicsState()
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: out))
print("Wrote \(out)")
