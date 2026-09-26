#!/usr/bin/env python3

import math

import bpy
from mathutils import Vector


def ngon(n, phase=0.0):
    return [(math.cos(phase + 2.0 * math.pi * i / n),
             math.sin(phase + 2.0 * math.pi * i / n)) for i in range(n)]


P8 = ngon(8)
P6 = ngon(6)
P4 = ngon(4, math.pi / 4.0)


def vring(z, rx, ry, y=0.0, x=0.0, profile=P8, mul=None, dz=None):
    out = []
    for i, (px, py) in enumerate(profile):
        m = mul[i] if mul else 1.0
        out.append((x + px * rx * m,
                    y + py * ry * m,
                    z + (dz[i] if dz else 0.0)))
    return out


def loft(rings, cap_first=True, cap_last=True):
    n = len(rings[0])
    verts = [v for ring in rings for v in ring]
    faces = []
    for i in range(len(rings) - 1):
        a, b = i * n, (i + 1) * n
        for j in range(n):
            k = (j + 1) % n
            faces.append((a + j, a + k, b + k, b + j))
    if cap_first:
        faces.append(tuple(reversed(range(n))))
    if cap_last:
        base = (len(rings) - 1) * n
        faces.append(tuple(base + j for j in range(n)))
    return verts, faces


def limb_rings(pivot, end, sections, profile=P6):
    p0, p1 = Vector(pivot), Vector(end)
    axis = p1 - p0
    length = axis.length
    w = axis / length
    ref = Vector((0.0, -1.0, 0.0))
    if abs(w.dot(ref)) > 0.95:
        ref = Vector((0.0, 0.0, 1.0))
    u = ref.cross(w).normalized()
    v = w.cross(u)
    return [[tuple(p0 + w * (t * length) + u * (px * rx) + v * (py * ry))
             for (px, py) in profile]
            for (t, rx, ry) in sections]


def ring_at(rings, z):
    zs = [r[0] for r in rings]
    lo = 0
    for i in range(len(zs) - 1):
        if zs[i] <= z:
            lo = i
    t = (z - zs[lo]) / (zs[lo + 1] - zs[lo])
    return [Vector(a).lerp(Vector(b), t)
            for a, b in zip(rings[lo][1], rings[lo + 1][1])]


def surface_point(rings, edge, s, z, out=0.0, scale=1.0):
    ring = ring_at(rings, z)
    n = len(ring)
    p0, p1 = ring[edge % n], ring[(edge + 1) % n]
    if scale != 1.0:
        p0 = Vector((p0.x * scale, p0.y * scale, p0.z))
        p1 = Vector((p1.x * scale, p1.y * scale, p1.z))
    point = p0.lerp(p1, s)
    if out:
        tangent = p1 - p0
        normal = Vector((tangent.y, -tangent.x, 0.0))
        if normal.length > 1e-9:
            point += normal.normalized() * out
    return tuple(point)


def merge(*parts):
    verts, faces, mats = [], [], []
    for pv, pf, pm in parts:
        base = len(verts)
        verts.extend(pv)
        faces.extend(tuple(i + base for i in f) for f in pf)
        mats.extend(pm if isinstance(pm, list) else [pm] * len(pf))
    return verts, faces, mats


def mirror_x(part):
    verts, faces, mats = part
    return ([(-x, y, z) for (x, y, z) in verts],
            [tuple(reversed(f)) for f in faces],
            list(mats) if isinstance(mats, list) else mats)


def fix_winding(verts, faces, ref):
    ref = Vector(ref)
    out = []
    for f in faces:
        pts = [Vector(verts[i]) for i in f]
        centroid = sum(pts, Vector()) / len(pts)
        normal = Vector()
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            normal += a.cross(b)
        out.append(tuple(reversed(f))
                   if normal.length > 1e-12 and normal.dot(centroid - ref) < 0.0
                   else f)
    return out


OBJECTS = {}
PIVOTS = {}
REST = {}


def bind_existing(scene, rest_frame):
    OBJECTS.clear()
    PIVOTS.clear()
    REST.clear()
    scene.frame_set(rest_frame)
    for obj in scene.objects:
        if obj.type != 'MESH':
            continue
        obj.rotation_mode = 'XYZ'
        OBJECTS[obj.name] = obj
        PIVOTS[obj.name] = Vector(obj.location)
        REST[obj.name] = (Vector(obj.location), Vector(obj.rotation_euler))
    return OBJECTS


def clear_animation(scene):
    for obj in scene.objects:
        obj.animation_data_clear()
        if obj.data and getattr(obj.data, "animation_data", None):
            obj.data.animation_data_clear()
    scene.timeline_markers.clear()


def reset_scene(fps=48, frame_start=1, frame_end=1):
    OBJECTS.clear()
    PIVOTS.clear()
    REST.clear()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = fps
    scene.render.fps_base = 1.0
    scene.frame_start = frame_start
    scene.frame_end = frame_end
    scene.unit_settings.system = 'METRIC'
    return scene


def build_materials(spec):
    out = []
    for name, base, specular in spec:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        principled = next(n for n in mat.node_tree.nodes
                          if n.type == 'BSDF_PRINCIPLED')
        principled.inputs["Base Color"].default_value = (*base, 1.0)
        principled.inputs["Roughness"].default_value = 0.85
        mat.diffuse_color = (*base, 1.0)
        mat.specular_color = specular
        mat.roughness = 0.85
        out.append(mat)
    return out


def add_part(name, part, pivot, parent=None, ref=None, materials=()):
    verts, faces, face_mats = part
    if not isinstance(face_mats, list):
        face_mats = [face_mats] * len(faces)
    pivot = Vector(pivot)
    faces = fix_winding(verts, faces, Vector(ref) if ref else pivot)

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([tuple(Vector(v) - pivot) for v in verts], [], faces)
    for mat in materials:
        mesh.materials.append(mat)
    mesh.validate(verbose=False)
    for poly, index in zip(mesh.polygons, face_mats):
        poly.material_index = index
    mesh.update()
    mesh.shade_flat()

    obj = bpy.data.objects.new(name, mesh)
    obj.rotation_mode = 'XYZ'
    if parent:
        obj.parent = OBJECTS[parent]
        obj.location = pivot - PIVOTS[parent]
    else:
        obj.location = pivot
    bpy.context.collection.objects.link(obj)

    OBJECTS[name] = obj
    PIVOTS[name] = pivot
    REST[name] = (Vector(obj.location), Vector(obj.rotation_euler))
    return obj


def add_anchor(name, parent, offset, rotation=(0.0, 0.0, 0.0)):
    empty = bpy.data.objects.new(name, None)
    empty.empty_display_type = 'ARROWS'
    empty.empty_display_size = 0.09
    empty.rotation_mode = 'XYZ'
    if parent:
        empty.parent = OBJECTS[parent]
    empty.location = Vector(offset)
    empty.rotation_euler = [math.radians(a) for a in rotation]
    bpy.context.collection.objects.link(empty)
    return empty


def triangle_count(scene):
    total = 0
    for obj in scene.objects:
        if obj.type == 'MESH':
            obj.data.calc_loop_triangles()
            total += len(obj.data.loop_triangles)
    return total
