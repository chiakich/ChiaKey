#!/usr/bin/env swift
import AppKit
import CoreGraphics

// Generates the menu bar template icon (native IME style: a solid rounded field
// with the 千 glyph knocked out). Drawn directly with CoreGraphics so the wide
// 22x16 aspect is independent of the square app icon (ChiaKey.svg / .icns).
// The glyph path below mirrors ChiaKey.svg's 千 silhouette.
// Usage: ./Scripts/generate-menu-icon.swift [output-dir]

let pathData = "M 454 909 C 485 909 499 891 499 852 L 499 463 L 856 463 C 892 463 910 450 910 420 C 910 391 891 377 856 377 L 499 377 L 499 135 C 650 120 738 105 766 96 C 807 82 816 47 795 21 C 778 0 743 8 711 17 C 596 49 316 76 119 77 C 84 77 57 91 57 120 C 57 149 74 163 108 163 C 154 163 254 158 408 145 L 408 377 L 53 377 C 17 377 0 390 0 419 C 0 449 18 463 53 463 L 408 463 L 408 852 C 408 891 423 909 454 909 Z"

// Tunables
let marginRatio: CGFloat = 0.0     // 0 → field fills the canvas edge-to-edge
let cornerRatio: CGFloat = 0.3     // corner radius relative to field height
let glyphFill:   CGFloat = 0.66    // glyph height relative to field height
let glyphScale:  CGFloat = 0.95    // fine size multiplier on top of glyphFill (1.0 = no change; ~0.9 shrinks ≈1px @1x)
let boldRatio:   CGFloat = 0.03    // stroke dilation of the glyph (thinner strokes)
let supersample = 4                // render this many times larger, then downscale for crisp edges

func makePath(_ d: String) -> CGMutablePath {
    let p = CGMutablePath()
    let sc = Scanner(string: d); sc.charactersToBeSkipped = CharacterSet(charactersIn: " ,\n\t")
    func n() -> CGFloat { var v = 0.0; sc.scanDouble(&v); return CGFloat(v) }
    var cmd: Character = " "
    while !sc.isAtEnd {
        if let c = sc.scanCharacters(from: CharacterSet(charactersIn: "MLCZmlcz")) { cmd = c.first! }
        switch cmd {
        case "M": p.move(to: CGPoint(x: n(), y: n()))
        case "L": p.addLine(to: CGPoint(x: n(), y: n()))
        case "C": let a = CGPoint(x: n(), y: n()); let b = CGPoint(x: n(), y: n()); let e = CGPoint(x: n(), y: n()); p.addCurve(to: e, control1: a, control2: b)
        case "Z", "z": p.closeSubpath()
        default: _ = sc.scanCharacter()
        }
    }
    return p
}

let glyph = makePath(pathData)
let bb = glyph.boundingBoxOfPath

// Resolve output directory: CLI arg, else <repo>/ChiaKey-Source/Loaders/OSX-IMK/Images.
let scriptDir = URL(fileURLWithPath: CommandLine.arguments[0]).deletingLastPathComponent()
let defaultDir = scriptDir.deletingLastPathComponent()
    .appendingPathComponent("ChiaKey-Source/Loaders/OSX-IMK/Images")
let outDir = CommandLine.arguments.count > 1 ? URL(fileURLWithPath: CommandLine.arguments[1]) : defaultDir

func newCtx(_ w: Int, _ h: Int) -> CGContext {
    CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: 0,
              space: CGColorSpaceCreateDeviceRGB(),
              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
}

// Render the icon at the given pixel size (already includes the supersample factor).
func render(_ W: Int, _ H: Int) -> CGImage {
    let ctx = newCtx(W, H)
    ctx.setShouldAntialias(true)
    ctx.clear(CGRect(x: 0, y: 0, width: W, height: H))
    ctx.translateBy(x: 0, y: CGFloat(H)); ctx.scaleBy(x: 1, y: -1)  // SVG-style y-down; glyph upright
    let fW = CGFloat(W), fH = CGFloat(H)

    let m = (fH * marginRatio).rounded()
    let boxH = fH - 2*m, boxW = fW - 2*m
    let r = boxH * cornerRatio
    let box = CGPath(roundedRect: CGRect(x: m, y: m, width: boxW, height: boxH),
                     cornerWidth: r, cornerHeight: r, transform: nil)

    let gh = boxH * glyphFill * glyphScale
    let s = gh / bb.height
    let gw = bb.width * s
    var gt = CGAffineTransform(translationX: (fW - gw)/2 - bb.minX*s,
                               y: m + (boxH - gh)/2 - bb.minY*s).scaledBy(x: s, y: s)
    let g = glyph.copy(using: &gt)!

    // Bold hole = glyph fill unioned with its stroked outline, cut out in a single clear pass.
    let hole = CGMutablePath()
    hole.addPath(g)
    if boldRatio > 0 {
        hole.addPath(g.copy(strokingWithWidth: fH * boldRatio, lineCap: .round, lineJoin: .round, miterLimit: 10))
    }

    ctx.addPath(box); ctx.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1)); ctx.fillPath()
    ctx.setBlendMode(.clear); ctx.addPath(hole); ctx.fillPath(); ctx.setBlendMode(.normal)
    return ctx.makeImage()!
}

func downscale(_ img: CGImage, _ W: Int, _ H: Int) -> CGImage {
    if img.width == W && img.height == H { return img }
    let ctx = newCtx(W, H)
    ctx.interpolationQuality = .high
    ctx.clear(CGRect(x: 0, y: 0, width: W, height: H))
    ctx.draw(img, in: CGRect(x: 0, y: 0, width: W, height: H))
    return ctx.makeImage()!
}

struct Target { let w: Int; let h: Int; let name: String }
let targets = [
    Target(w: 22, h: 16, name: "ChiaKeyMenuIcon.png"),
    Target(w: 44, h: 32, name: "ChiaKeyMenuIcon@2x.png"),
]

for t in targets {
    let big = render(t.w * supersample, t.h * supersample)
    let img = downscale(big, t.w, t.h)
    let rep = NSBitmapImageRep(cgImage: img); rep.size = NSSize(width: t.w, height: t.h)
    guard let png = rep.representation(using: .png, properties: [:]) else { continue }
    let url = outDir.appendingPathComponent(t.name)
    try? png.write(to: url)
    print("wrote \(url.path) (\(t.w)x\(t.h))")
}
