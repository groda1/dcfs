#!/usr/bin/env python3

import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from modelkit import (P4, P6, add_anchor, add_part, build_materials,
                      loft, merge, reset_scene, triangle_count, vring)

MATERIALS = [
    ("steel",   (0.62, 0.65, 0.70), (0.85, 0.88, 0.92)),
    ("leather", (0.22, 0.15, 0.11), (0.05, 0.04, 0.04)),
    ("brass",   (0.58, 0.45, 0.20), (0.70, 0.60, 0.32)),
]
MAT = {name: i for i, (name, _, _) in enumerate(MATERIALS)}


def build_sword(materials):
    blade = [
        vring(0.052, 0.009, 0.021, profile=P4),
        vring(0.090, 0.011, 0.032, profile=P4),
        vring(0.430, 0.009, 0.028, profile=P4),
        vring(0.650, 0.006, 0.020, profile=P4),
        vring(0.722, 0.002, 0.004, profile=P4),
    ]

    guard = [
        vring(0.040, 0.016, 0.098, profile=P4, mul=[1.0, 0.30, 1.0, 0.30]),
        vring(0.058, 0.013, 0.104, profile=P4, mul=[1.0, 0.26, 1.0, 0.26]),
        vring(0.070, 0.008, 0.088, profile=P4, mul=[1.0, 0.22, 1.0, 0.22]),
    ]

    grip = [
        vring(-0.086, 0.013, 0.016, profile=P6),
        vring(-0.030, 0.015, 0.019, profile=P6),
        vring(0.040, 0.013, 0.017, profile=P6),
    ]

    pommel = [
        vring(-0.124, 0.010, 0.010, profile=P6),
        vring(-0.108, 0.026, 0.028, profile=P6),
        vring(-0.086, 0.020, 0.022, profile=P6),
    ]

    add_part("sword",
             merge((*loft(blade), MAT["steel"]),
                   (*loft(guard), MAT["brass"]),
                   (*loft(grip), MAT["leather"]),
                   (*loft(pommel), MAT["brass"])),
             (0.0, 0.0, 0.0), ref=(0.0, 0.0, 0.0), materials=materials)


def parse_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []
    out, force = None, False
    for arg in argv:
        if arg == "--force":
            force = True
        elif out is None:
            out = arg
        else:
            raise SystemExit(f"unexpected argument: {arg}")
    return out or "resources/models/blender/sword.blend", force


def main():
    out_path, force = parse_args()
    if os.path.exists(out_path) and not force:
        raise SystemExit(f"{out_path} already exists; pass --force to regenerate.")

    scene = reset_scene()
    build_sword(build_materials(MATERIALS))

    add_anchor("grip", None, (0.0, 0.0, 0.0))

    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out_path))
    print(f"\nwrote {out_path}: {triangle_count(scene)} triangles, "
          f"{len(MATERIALS)} palette slots, static (no animation)")


if __name__ == "__main__":
    main()
