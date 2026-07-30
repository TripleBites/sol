#!/usr/bin/env python3
"""
Sol UI System — Python Example
================================

This example demonstrates the Godot-inspired retained-mode UI system
built into the Sol engine. No window is opened; we create a UI scene,
run the layout solver, and inspect the draw commands that a renderer
backend would consume.

Structure built:

    SceneTree
      └─ Control "root" (800×600)
           ├─ ColorRect "sky"       — full-rect, dark blue background
           └─ VBoxContainer "panel" — centred, 300×400, with 3 children:
                ├─ ColorRect "header"  — red, 300×80
                ├─ ColorRect "body"    — green, expands to fill
                └─ ColorRect "footer"  — blue, 300×60

Run:
    PYTHONPATH=src python3 examples/ui_example.py
"""

import sys
import os

# Ensure we can find the sol package
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))

from sol.ui_bindings import (
    Control,
    ColorRect,
    VBoxContainer,
    SceneTree,
    SIZE_FILL,
    SIZE_EXPAND,
    DRAW_CMD_RECT_FILLED,
    DRAW_CMD_CLIP_PUSH,
    DRAW_CMD_CLIP_POP,
    _draw_list_cmd_count,
    _draw_list_get_cmd,
    _DRAW_CMD_NAMES,
)


def main():
    print("=" * 60)
    print("  Sol UI System — Python Example")
    print("=" * 60)
    print()

    # ------------------------------------------------------------------
    # 1. Create the scene tree
    # ------------------------------------------------------------------
    tree = SceneTree()
    print("✓ SceneTree created")

    # ------------------------------------------------------------------
    # 2. Root control (the canvas)
    # ------------------------------------------------------------------
    root = Control()
    root.set_name("root")
    # Give the root an initial rect (acts as the screen)
    # We set this directly via the C struct through offsets and anchors
    root.set_anchor(0, 0, 0, 0)  # top-left anchor
    root.set_margin(0, 0, 800, 600)  # fixed size 800×600
    print("✓ Root Control created (800×600)")

    # ------------------------------------------------------------------
    # 3. Background — full-rect ColorRect (dark blue)
    # ------------------------------------------------------------------
    sky = ColorRect()
    sky.set_name("sky")
    sky.set_color(0.05, 0.05, 0.2, 1.0)  # dark navy blue
    sky.set_anchor(0, 0, 1, 1)  # full rect
    sky.set_margin(0, 0, 0, 0)
    root.add_child(sky)
    print("✓ Background ColorRect added (full rect, dark blue)")

    # ------------------------------------------------------------------
    # 4. Centered panel — VBoxContainer with 3 children
    # ------------------------------------------------------------------
    panel = VBoxContainer()
    panel.set_name("panel")
    # Centre it: anchors at center, negative offsets for half width/height
    panel.set_anchor(0.5, 0.5, 0.5, 0.5)
    panel.set_margin(-150, -200, 150, 200)  # 300×400 centred
    panel.set_separation(8)  # 8px gap between children

    # 4a. Header bar (red)
    header = ColorRect()
    header.set_name("header")
    header.set_color(0.8, 0.2, 0.2, 1.0)  # red
    header.set_min_size(0, 80)
    header.set_size_flags(SIZE_FILL, 0)  # fill horizontally
    panel.add_child(header)

    # 4b. Body (green, expands to fill remaining space)
    body = ColorRect()
    body.set_name("body")
    body.set_color(0.2, 0.7, 0.2, 1.0)  # green
    body.set_size_flags(SIZE_FILL, SIZE_EXPAND | SIZE_FILL)  # fill both, expand vertically
    panel.add_child(body)

    # 4c. Footer bar (blue)
    footer = ColorRect()
    footer.set_name("footer")
    footer.set_color(0.2, 0.3, 0.9, 1.0)  # blue
    footer.set_min_size(0, 60)
    footer.set_size_flags(SIZE_FILL, 0)  # fill horizontally
    panel.add_child(footer)

    root.add_child(panel)
    print("✓ Panel VBoxContainer with 3 children added")
    print("    ├─ header (red, 80px)")
    print("    ├─ body   (green, expand)")
    print("    └─ footer (blue, 60px)")

    # ------------------------------------------------------------------
    # 5. Set root and process
    # ------------------------------------------------------------------
    tree.set_root(root)
    print("\n--- Tree structure (before layout) ---")
    tree.process(0.016)

    # ------------------------------------------------------------------
    # 6. Print results
    # ------------------------------------------------------------------
    print("\n--- Draw commands ---")
    tree.print_commands()

    # ------------------------------------------------------------------
    # 7. Verify expectations
    # ------------------------------------------------------------------
    print("\n--- Layout verification ---")
    dl = tree.get_draw_list()
    n = _draw_list_cmd_count(dl)

    # Collect rect_filled commands to verify
    fills = []
    for i in range(n):
        cmd = _draw_list_get_cmd(dl, i)
        if cmd.type == DRAW_CMD_RECT_FILLED:
            fills.append(cmd)

    expected = [
        ("sky", 0, 0, 800, 600, (0.05, 0.05, 0.20)),
        ("header", 250, 100, 300, 80, (0.80, 0.20, 0.20)),
        ("body", 250, 188, 300, 244, (0.20, 0.70, 0.20)),
        ("footer", 250, 440, 300, 60, (0.20, 0.30, 0.90)),
    ]

    all_ok = True
    for i, (name, x, y, w, h, c) in enumerate(expected):
        if i < len(fills):
            f = fills[i]
            rx, ry, rw, rh = int(f.rect.x), int(f.rect.y), int(f.rect.w), int(f.rect.h)
            ok = rx == x and ry == y and rw == w and rh == h
            status = "✓" if ok else "✗"
            if not ok:
                all_ok = False
            print(
                f"  {status} {name:10s} "
                f"expected ({x:4d},{y:4d},{w:4d},{h:4d}) "
                f"got ({rx:4d},{ry:4d},{rw:4d},{rh:4d})"
            )
        else:
            print(f"  ✗ {name:10s} MISSING")
            all_ok = False

    if all_ok:
        print("\n  ✓ All layout checks passed!")
    else:
        print("\n  ⚠ Some layout checks failed (may be due to anchor computation)")

    # ------------------------------------------------------------------
    # 8. The tree is cleaned up when Python objects go out of scope.
    #    SceneTree.__del__ destroys the C tree, which cascades to nodes.
    # ------------------------------------------------------------------
    print("\nDone. SceneTree and all nodes cleaned up.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
