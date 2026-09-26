#!/usr/bin/env python3

import math
import sys

import bpy
from mathutils import Vector

PAD = 1.0

VIEWS = {
    "front":   ((0.0, -1.0, 0.0), True),
    "back":    ((0.0, 1.0, 0.0), True),
    "side":    ((-1.0, 0.0, 0.0), True),
    "quarter": ((-0.80, -0.95, 0.42), True),
    "game":    ((0.0, -0.42, 0.91), False),
    "top":     ((0.0, 0.0, 1.0), True),
}


def parse_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []
    out = None
    frames = None
    views = ["front", "side", "quarter"]
    res = (480, 720)
    attach = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--frames":
            i += 1
            frames = [int(f) for f in argv[i].split(",")]
        elif arg == "--views":
            i += 1
            views = argv[i].split(",")
        elif arg == "--res":
            i += 1
            w, h = argv[i].split("x")
            res = (int(w), int(h))
        elif arg == "--pad":
            i += 1
            globals()["PAD"] = float(argv[i])
        elif arg == "--attach":
            i += 1
            path, _, anchor = argv[i].rpartition(":")
            if not path:
                raise SystemExit("--attach wants <blend>:<anchor name>")
            attach.append((path, anchor))
        elif out is None:
            out = arg
        else:
            raise SystemExit(f"unexpected argument: {arg}")
        i += 1
    return (out or "/tmp/preview", frames or [bpy.context.scene.frame_start],
            views, res, attach)


def attach_model(scene, path, anchor_name):
    anchor = scene.objects.get(anchor_name)
    if anchor is None or anchor.type != 'EMPTY':
        raise SystemExit(f"no anchor empty named {anchor_name!r} in the scene")

    with bpy.data.libraries.load(path) as (src, dst):
        dst.objects = [n for n in src.objects]

    for obj in dst.objects:
        if obj is None or obj.type != 'MESH':
            continue
        scene.collection.objects.link(obj)
        if obj.parent is None:
            obj.parent = anchor
            obj.matrix_parent_inverse.identity()
        print(f"attached {obj.name!r} to anchor {anchor_name!r}")


def bounds(scene):
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for obj in scene.objects:
        if obj.type != 'MESH':
            continue
        for corner in obj.bound_box:
            p = obj.matrix_world @ Vector(corner)
            lo = Vector((min(lo[i], p[i]) for i in range(3)))
            hi = Vector((max(hi[i], p[i]) for i in range(3)))
    return lo, hi


def setup(scene, res):
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    scene.render.image_settings.file_format = 'PNG'

    shading = scene.display.shading
    shading.light = 'STUDIO'
    shading.color_type = 'MATERIAL'
    shading.show_shadows = True
    shading.shadow_intensity = 0.4
    shading.show_cavity = True
    shading.cavity_type = 'BOTH'
    shading.show_object_outline = False
    shading.studiolight_rotate_z = math.radians(-35.0)
    scene.display.render_aa = '8'
    scene.world = bpy.data.worlds.new("preview") if scene.world is None else scene.world
    scene.world.color = (0.20, 0.21, 0.24)

    cam_data = bpy.data.cameras.new("preview_cam")
    cam = bpy.data.objects.new("preview_cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam


def aim(cam, direction, center, radius, ortho):
    d = Vector(direction).normalized()
    cam.data.type = 'ORTHO' if ortho else 'PERSP'
    if ortho:
        cam.data.ortho_scale = radius * 2.15 * PAD
        cam.location = center + d * (radius * 6.0)
    else:
        cam.data.lens = 55.0
        cam.location = center + d * (radius * 3.4 * PAD)
    fwd = (center - cam.location).normalized()
    cam.rotation_euler = fwd.to_track_quat('-Z', 'Y').to_euler()


def main():
    out, frames, views, res, attach = parse_args()
    scene = bpy.context.scene
    cam = setup(scene, res)

    for path, anchor_name in attach:
        attach_model(scene, path, anchor_name)

    lo, hi = bounds(scene)
    center = (lo + hi) * 0.5
    radius = max((hi - lo).x, (hi - lo).y, (hi - lo).z) * 0.5

    for frame in frames:
        scene.frame_set(frame)
        for view in views:
            direction, ortho = VIEWS[view]
            aim(cam, direction, center, radius, ortho)
            scene.render.filepath = f"{out}_{view}_{frame:03d}.png"
            bpy.ops.render.render(write_still=True)
            print(f"rendered {scene.render.filepath}")


if __name__ == "__main__":
    main()
