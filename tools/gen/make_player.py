#!/usr/bin/env python3

import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import animate_player
from modelkit import (P4, P6, P8, OBJECTS, PIVOTS, add_anchor, add_part,
                      build_materials, limb_rings, loft, merge, mirror_x,
                      reset_scene, ring_at, surface_point, triangle_count,
                      vring)


MATERIALS = [
    ("skin",  (0.78, 0.58, 0.45), (0.10, 0.09, 0.08)),
    ("hair",  (0.17, 0.11, 0.08), (0.08, 0.07, 0.06)),
    ("eye",   (0.10, 0.10, 0.13), (0.30, 0.30, 0.30)),
    ("mouth", (0.38, 0.20, 0.18), (0.00, 0.00, 0.00)),
]
MAT = {name: i for i, (name, _, _) in enumerate(MATERIALS)}


SHOULDER = Vector((-0.200, 0.004, 1.425))
ELBOW    = Vector((-0.230, -0.014, 1.105))
WRIST    = Vector((-0.250, -0.032, 0.858))
FINGERS  = Vector((-0.258, -0.052, 0.724))

HIP      = Vector((-0.094, 0.000, 0.896))
KNEE     = Vector((-0.100, -0.004, 0.472))
ANKLE    = Vector((-0.104, 0.012, 0.090))
TOE      = Vector((-0.108, -0.205, 0.028))

PELVIS_PIVOT = Vector((0.0, 0.0, 1.000))
TORSO_PIVOT  = Vector((0.0, 0.0, 1.100))
NECK_PIVOT   = Vector((0.0, 0.004, 1.540))


PELVIS_RINGS = [
    (0.838, vring(0.838, 0.126, 0.098, y=0.014)),
    (0.898, vring(0.898, 0.198, 0.134, y=0.012,
                  mul=[1.0, 1.10, 1.24, 1.10, 1.0, 1.0, 0.96, 1.0])),
    (1.020, vring(1.020, 0.183, 0.119, y=0.004,
                  mul=[1.0, 1.04, 1.09, 1.04, 1.0, 1.0, 1.0, 1.0])),
    (1.145, vring(1.145, 0.158, 0.104)),
]


def build_pelvis(materials):
    verts, faces = loft([r[1] for r in PELVIS_RINGS])
    add_part("m_pelvis", (verts, faces, MAT["skin"]), PELVIS_PIVOT,
             materials=materials)


def build_torso(materials):
    rings = [
        vring(1.045, 0.156, 0.104),
        vring(1.125, 0.148, 0.100),
        vring(1.245, 0.184, 0.119),
        vring(1.338, 0.208, 0.136, y=-0.008,
              mul=[1.0, 1.0, 0.92, 1.0, 1.0, 1.03, 1.0, 1.03]),
        vring(1.420, 0.197, 0.112),
        vring(1.472, 0.124, 0.084, y=0.004),
        vring(1.556, 0.064, 0.066, y=0.006),
    ]
    verts, faces = loft(rings)
    add_part("m_torso", (verts, faces, MAT["skin"]), TORSO_PIVOT,
             parent="m_pelvis", ref=(0.0, 0.0, 1.26), materials=materials)


HEAD_RINGS = [
    (1.504, vring(1.504, 0.078, 0.084, y=0.022)),
    (1.560, vring(1.560, 0.109, 0.111, y=0.008,
                  mul=[1.0, 0.98, 0.93, 0.98, 1.0, 1.03, 1.09, 1.03])),
    (1.632, vring(1.632, 0.118, 0.125, y=-0.002)),
    (1.712, vring(1.712, 0.117, 0.124, y=0.002)),
    (1.786, vring(1.786, 0.101, 0.108, y=0.008)),
    (1.814, vring(1.814, 0.050, 0.057, y=0.014)),
]

HEAD_CENTER = Vector((0.0, 0.006, 1.660))


def head_point(edge, s, z, out=0.0):
    return surface_point(HEAD_RINGS, edge, s, z, out)


def build_head(materials):
    verts, faces = loft([r[1] for r in HEAD_RINGS])
    parts = [(verts, faces, MAT["skin"])]

    for edge, s0, s1 in ((5, 0.14, 0.62), (6, 0.38, 0.86)):
        parts.append(([head_point(edge, s0, 1.672, 0.003),
                       head_point(edge, s1, 1.672, 0.003),
                       head_point(edge, s1, 1.702, 0.003),
                       head_point(edge, s0, 1.702, 0.003)],
                      [(0, 1, 2, 3)], MAT["eye"]))
        parts.append(([head_point(edge, s0 - 0.05, 1.714, 0.004),
                       head_point(edge, s1 + 0.03, 1.714, 0.004),
                       head_point(edge, s1 + 0.03, 1.731, 0.004),
                       head_point(edge, s0 - 0.05, 1.731, 0.004)],
                      [(0, 1, 2, 3)], MAT["hair"]))

    base = [head_point(5, 0.78, 1.714), head_point(6, 0.22, 1.714),
            head_point(6, 0.18, 1.606), head_point(5, 0.82, 1.606)]
    apex = head_point(5, 1.0, 1.652, 0.042)
    parts.append((base + [apex],
                  [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (0, 3, 2, 1)],
                  MAT["skin"]))

    parts.append(([head_point(5, 0.64, 1.570, 0.002), head_point(6, 0.36, 1.570, 0.002),
                   head_point(6, 0.36, 1.584, 0.002), head_point(5, 0.64, 1.584, 0.002)],
                  [(0, 1, 2, 3)], MAT["mouth"]))

    add_part("m_head", merge(*parts), NECK_PIVOT, parent="m_torso",
             ref=HEAD_CENTER, materials=materials)


def build_hair(materials):
    rim_dz = [-0.078, -0.128, -0.148, -0.128, -0.078, -0.016, 0.0, -0.016]
    rings = [
        vring(1.748, 0.122, 0.130, y=0.002, dz=rim_dz,
              mul=[1.0, 1.0, 1.0, 1.0, 1.0, 0.97, 0.94, 0.97]),
        vring(1.786, 0.120, 0.128, y=0.006),
        vring(1.812, 0.094, 0.101, y=0.010),
        vring(1.832, 0.046, 0.052, y=0.014),
    ]
    verts, faces = loft(rings)
    add_part("m_hair", (verts, faces, MAT["hair"]), NECK_PIVOT,
             parent="m_head", ref=(0.0, 0.01, 1.75), materials=materials)


def build_mustache(materials):
    sweep = [(5, 0.22), (5, 0.58), (5, 1.00), (6, 0.42), (6, 0.78)]
    droop = [-0.024, -0.008, 0.0, -0.008, -0.024]
    thick = [0.015, 0.026, 0.032, 0.026, 0.015]
    top, bot = 1.618, 1.582

    rings = []
    for (edge, s), dz, out in zip(sweep, droop, thick):
        rings.append([head_point(edge, s, top + dz, 0.002),
                      head_point(edge, s, (top + bot) * 0.5 + dz, out),
                      head_point(edge, s, bot + dz, 0.002)])
    verts, faces = loft(rings)
    add_part("m_mustache", (verts, faces, MAT["hair"]),
             (0.0, -0.100, 1.605), parent="m_head",
             ref=(0.0, -0.02, 1.605), materials=materials)


def build_arm(materials, side, suffix):
    def flip(part):
        return mirror_x(part) if side > 0 else part

    shoulder = Vector((SHOULDER.x * (-1 if side > 0 else 1), SHOULDER.y, SHOULDER.z))
    elbow    = Vector((ELBOW.x * (-1 if side > 0 else 1), ELBOW.y, ELBOW.z))
    wrist    = Vector((WRIST.x * (-1 if side > 0 else 1), WRIST.y, WRIST.z))
    fingers  = Vector((FINGERS.x * (-1 if side > 0 else 1), FINGERS.y, FINGERS.z))

    upper = limb_rings(SHOULDER, ELBOW, [
        (-0.19, 0.038, 0.037),
        (-0.10, 0.068, 0.065),
        (0.06, 0.084, 0.080),
        (0.55, 0.064, 0.061),
        (1.00, 0.055, 0.053),
    ])
    add_part(f"m_arm_upper_{suffix}", flip(merge((*loft(upper), MAT["skin"]))),
             shoulder, parent="m_torso",
             ref=(shoulder + elbow) * 0.5, materials=materials)

    lower = limb_rings(ELBOW, WRIST, [
        (-0.09, 0.058, 0.056),
        (0.32, 0.055, 0.053),
        (1.00, 0.038, 0.042),
    ])
    add_part(f"m_arm_lower_{suffix}", flip(merge((*loft(lower), MAT["skin"]))),
             elbow, parent=f"m_arm_upper_{suffix}",
             ref=(elbow + wrist) * 0.5, materials=materials)

    fist = limb_rings(WRIST, FINGERS, [
        (-0.12, 0.039, 0.043),
        (0.30, 0.048, 0.058),
        (0.82, 0.046, 0.056),
        (1.00, 0.032, 0.041),
    ])
    thumb = limb_rings((-0.216, -0.056, 0.822), (-0.214, -0.070, 0.766),
                       [(0.0, 0.021, 0.021), (1.0, 0.014, 0.014)], profile=P4)
    hand = merge((*loft(fist), MAT["skin"]), (*loft(thumb), MAT["skin"]))
    add_part(f"m_hand_{suffix}", flip(hand), wrist,
             parent=f"m_arm_lower_{suffix}",
             ref=(wrist + fingers) * 0.5, materials=materials)


def build_leg(materials, side, suffix):
    def flip(part):
        return mirror_x(part) if side > 0 else part

    hip   = Vector((HIP.x * (-1 if side > 0 else 1), HIP.y, HIP.z))
    knee  = Vector((KNEE.x * (-1 if side > 0 else 1), KNEE.y, KNEE.z))
    ankle = Vector((ANKLE.x * (-1 if side > 0 else 1), ANKLE.y, ANKLE.z))
    toe   = Vector((TOE.x * (-1 if side > 0 else 1), TOE.y, TOE.z))

    thigh = limb_rings(HIP, KNEE, [
        (-0.12, 0.092, 0.090),
        (0.14, 0.112, 0.118),
        (0.52, 0.100, 0.106),
        (1.00, 0.079, 0.085),
    ])
    add_part(f"m_leg_upper_{suffix}", flip(merge((*loft(thigh), MAT["skin"]))),
             hip, parent="m_pelvis", ref=(hip + knee) * 0.5, materials=materials)

    shin = limb_rings(KNEE, ANKLE, [
        (-0.11, 0.081, 0.087),
        (0.30, 0.074, 0.082),
        (1.00, 0.045, 0.051),
    ])
    add_part(f"m_leg_lower_{suffix}", flip(merge((*loft(shin), MAT["skin"]))),
             knee, parent=f"m_leg_upper_{suffix}",
             ref=(knee + ankle) * 0.5, materials=materials)

    def section(y, half_w, top):
        return [(ANKLE.x - half_w, y, 0.0),
                (ANKLE.x + half_w, y, 0.0),
                (ANKLE.x + half_w, y, top * 0.62),
                (ANKLE.x + half_w * 0.58, y, top),
                (ANKLE.x - half_w * 0.58, y, top),
                (ANKLE.x - half_w, y, top * 0.62)]

    sections = [section(0.050, 0.046, 0.112),
                section(-0.074, 0.052, 0.092),
                section(-0.160, 0.049, 0.050),
                section(-0.205, 0.039, 0.030)]
    add_part(f"m_foot_{suffix}", flip(merge((*loft(sections), MAT["skin"]))),
             ankle, parent=f"m_leg_lower_{suffix}",
             ref=(ANKLE.x * (-1 if side > 0 else 1), -0.07, 0.045),
             materials=materials)


def build_anchors():
    add_anchor("hand.r", "m_hand_r", (0.014, -0.012, -0.050), (148.0, 0.0, -7.0))
    add_anchor("hand.l", "m_hand_l", (-0.014, -0.012, -0.050), (148.0, 0.0, 7.0))
    add_anchor("head",   "m_head",   (0.0, 0.004, 0.238), (0.0, 0.0, 0.0))
    add_anchor("torso",  "m_torso",  (0.0, -0.006, 0.245), (0.0, 0.0, 0.0))
    add_anchor("back",   "m_torso",  (0.0, 0.104, 0.215), (0.0, 0.0, 180.0))
    add_anchor("foot.r", "m_foot_r", (0.0, -0.052, 0.006), (0.0, 0.0, 0.0))
    add_anchor("foot.l", "m_foot_l", (0.0, -0.052, 0.006), (0.0, 0.0, 0.0))


def parse_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []
    out = None
    force = False
    for arg in argv:
        if arg == "--force":
            force = True
        elif out is None:
            out = arg
        else:
            raise SystemExit(f"unexpected argument: {arg}")
    return out or "resources/models/blender/player.blend", force


def main():
    out_path, force = parse_args()

    if os.path.exists(out_path) and not force:
        raise SystemExit(
            f"{out_path} already exists.\n"
            "This script REGENERATES the model from scratch and will discard\n"
            "anything you edited by hand. Pass --force if that is what you want.")

    scene = reset_scene(fps=animate_player.FPS,
                        frame_start=animate_player.FRAME_START,
                        frame_end=animate_player.FRAME_END)
    materials = build_materials(MATERIALS)

    build_pelvis(materials)
    build_torso(materials)
    build_head(materials)
    build_hair(materials)
    build_mustache(materials)
    for side, suffix in ((-1, "r"), (1, "l")):
        build_arm(materials, side, suffix)
        build_leg(materials, side, suffix)
    build_anchors()

    animate_player.apply(scene)

    tris = triangle_count(scene)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out_path))

    print()
    print("=" * 70)
    print(f"wrote {out_path}")
    print(f"  {len([o for o in scene.objects if o.type == 'MESH'])} mesh objects, "
          f"{tris} triangles")
    print(f"  {len([o for o in scene.objects if o.type == 'EMPTY'])} anchors: "
          f"{', '.join(sorted(o.name for o in scene.objects if o.type == 'EMPTY'))}")
    print(f"  {len(MATERIALS)} palette slots: {', '.join(m[0] for m in MATERIALS)}")
    print("=" * 70)


if __name__ == "__main__":
    main()
