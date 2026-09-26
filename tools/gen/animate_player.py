#!/usr/bin/env python3
"""
Applies the player's animations to an existing .blend.

    blender -b resources/models/blender/player.blend --python-exit-code 1 \
        -P tools/gen/animate_player.py -- --save

Geometry belongs to the .blend and this script never touches it. The pose
tables only set rotation_euler, plus location on the root, on objects looked
up BY NAME. Reshape a mesh, move an origin, retarget a material, parent a
new rigid child onto a segment (a ponytail on m_head, a belt on m_pelvis) --
none of that is visible here, and none of it needs a change below.

Re-running is idempotent. Existing actions and markers are wiped first so
nothing accumulates, and the rest pose is read back at REST_FRAME, where
every authored pose is zero. That means poses are applied ON TOP of whatever
rest rotation an object carries: rotate a shoulder in the viewport to change
the stance and the offset survives every animation.

What does need a change here: renaming or reparenting a segment (fails loud
with a KeyError), and splitting a segment into a new independently moving
joint (add its name to ANIMATED; existing poses leave it at zero).

make_player.py imports apply() to animate a scene it just built.

Joint signs, which are not symmetric and are easy to get backwards:
  positive x swings a limb BACKWARD, so a forward step is negative on a
  thigh; a knee bend is POSITIVE on a shin, because knees fold backward; an
  elbow bend is NEGATIVE on a forearm, because elbows fold forward. Positive
  z yaws toward the character's LEFT (+X). Root translation is in engine
  units with -y forward.
"""

import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from modelkit import (OBJECTS, REST, bind_existing, clear_animation)   # noqa: E402

FPS = 48

IDLE_START,   IDLE_END   = 1, 48
WALK_R_START, WALK_R_END = 49, 61
WALK_L_START, WALK_L_END = 62, 74
ATTACK_START, ATTACK_END = 75, 105
BUMP_START,   BUMP_END   = 106, 124

REST_FRAME = IDLE_START
FRAME_START, FRAME_END = IDLE_START, BUMP_END

STEP_FRAMES = WALK_R_END - WALK_R_START

ANIMATED = [
    "m_pelvis", "m_torso", "m_head",
    "m_arm_upper_r", "m_arm_lower_r", "m_hand_r",
    "m_arm_upper_l", "m_arm_lower_l", "m_hand_l",
    "m_leg_upper_r", "m_leg_lower_r", "m_foot_r",
    "m_leg_upper_l", "m_leg_lower_l", "m_foot_l",
]


def key(name, frame, rot=(0.0, 0.0, 0.0), loc=None):
    obj = OBJECTS[name]
    base_loc, base_rot = REST[name]
    obj.rotation_euler = [base_rot[i] + math.radians(rot[i]) for i in range(3)]
    obj.keyframe_insert("rotation_euler", frame=frame)
    if loc is not None:
        obj.location = base_loc + Vector(loc)
        obj.keyframe_insert("location", frame=frame)


def pose(frame, **parts):
    root = parts.pop("root", None)
    key("m_pelvis", frame, parts.pop("pelvis", (0.0, 0.0, 0.0)),
        loc=root if root is not None else (0.0, 0.0, 0.0))
    for name in ANIMATED:
        if name == "m_pelvis":
            continue
        key(name, frame, parts.pop(name[2:], (0.0, 0.0, 0.0)))
    if parts:
        raise SystemExit(f"pose({frame}): unknown segment(s) {sorted(parts)}")


def build_idle():
    a, b = IDLE_START, IDLE_END
    m1, m2, m3 = a + 12, a + 24, a + 36

    pose(a,  root=(0, 0, 0))
    pose(m1, root=(0, 0, 0.007), torso=(-1.4, 0, 0), head=(0.8, 0, 0),
         arm_upper_r=(-1.5, 0, -2.0), arm_upper_l=(-1.5, 0, 2.0))
    pose(m2, root=(0, 0, 0.010), torso=(-2.0, 0, 0), head=(1.2, 0, 0),
         arm_upper_r=(-2.2, 0, -3.0), arm_upper_l=(-2.2, 0, 3.0))
    pose(m3, root=(0, 0, 0.004), torso=(-0.8, 0, 0), head=(0.5, 0, 0),
         arm_upper_r=(-1.0, 0, -1.5), arm_upper_l=(-1.0, 0, 1.5))
    pose(b,  root=(0, 0, 0))


def mirrored(parts):
    """A pose reflected across x = 0: left and right swap, and yaw/roll flip.
    walk_l is walk_r through this, which is what guarantees the two chain --
    each one's end pose is literally the other one's start pose."""
    out = {}
    for name, v in parts.items():
        if name == "root":
            out[name] = (-v[0], v[1], v[2])
        elif name.endswith("_r"):
            out[name[:-2] + "_l"] = (v[0], -v[1], -v[2])
        elif name.endswith("_l"):
            out[name[:-2] + "_r"] = (v[0], -v[1], -v[2])
        else:
            out[name] = (v[0], -v[1], -v[2])
    return out


STEP = [
    (0, dict(
        root=(0, 0, 0.010),
        leg_upper_r=(-9, 0, 0), leg_lower_r=(36, 0, 0), foot_r=(-12, 0, 0),
        leg_upper_l=(7, 0, 0), leg_lower_l=(4, 0, 0),
        arm_upper_r=(6, 0, -2), arm_lower_r=(-12, 0, 0),
        arm_upper_l=(-8, 0, 2), arm_lower_l=(-16, 0, 0),
        torso=(2, 0, -1), head=(0, 0, 1))),

    (2, dict(
        root=(0, 0, 0.003),
        leg_upper_r=(-21, 0, 0), leg_lower_r=(27, 0, 0), foot_r=(-7, 0, 0),
        leg_upper_l=(14, 0, 0), leg_lower_l=(7, 0, 0), foot_l=(-3, 0, 0),
        arm_upper_r=(12, 0, -3), arm_lower_r=(-10, 0, 0),
        arm_upper_l=(-16, 0, 3), arm_lower_l=(-19, 0, 0),
        torso=(3, 0, -2), head=(0, 0, 2))),

    (4, dict(
        root=(0, 0, -0.011),
        leg_upper_r=(-30, 0, 0), leg_lower_r=(13, 0, 0),
        leg_upper_l=(21, 0, 0), leg_lower_l=(11, 0, 0), foot_l=(-9, 0, 0),
        arm_upper_r=(18, 0, -4), arm_lower_r=(-8, 0, 0),
        arm_upper_l=(-22, 0, 4), arm_lower_l=(-21, 0, 0),
        torso=(4, 0, -3), head=(-1, 0, 3))),

    (6, dict(
        root=(0, 0, -0.022),
        leg_upper_r=(-32, 0, 0), leg_lower_r=(4, 0, 0), foot_r=(8, 0, 0),
        leg_upper_l=(26, 0, 0), leg_lower_l=(17, 0, 0), foot_l=(-20, 0, 0),
        arm_upper_r=(21, 0, -4), arm_lower_r=(-7, 0, 0),
        arm_upper_l=(-25, 0, 4), arm_lower_l=(-23, 0, 0),
        torso=(5, 0, -4), head=(-1, 0, 4))),

    (8, dict(
        root=(0, 0, -0.013),
        leg_upper_r=(-19, 0, 0), leg_lower_r=(4, 0, 0), foot_r=(5, 0, 0),
        leg_upper_l=(16, 0, 0), leg_lower_l=(32, 0, 0), foot_l=(-15, 0, 0),
        arm_upper_r=(14, 0, -3), arm_lower_r=(-10, 0, 0),
        arm_upper_l=(-15, 0, 3), arm_lower_l=(-19, 0, 0),
        torso=(4, 0, -2), head=(-1, 0, 2))),

    (10, dict(
        root=(0, 0, 0.002),
        leg_upper_r=(-5, 0, 0), leg_lower_r=(7, 0, 0),
        leg_upper_l=(2, 0, 0), leg_lower_l=(40, 0, 0), foot_l=(-15, 0, 0),
        arm_upper_r=(3, 0, -2), arm_lower_r=(-13, 0, 0),
        arm_upper_l=(-3, 0, 2), arm_lower_l=(-17, 0, 0),
        torso=(3, 0, 0), head=(0, 0, 0))),
]


def build_step(start, flip):
    for offset, parts in STEP:
        pose(start + offset, **(mirrored(parts) if flip else parts))
    pose(start + STEP_FRAMES,
         **(STEP[0][1] if flip else mirrored(STEP[0][1])))


def build_walk_r():
    build_step(WALK_R_START, flip=False)


def build_walk_l():
    build_step(WALK_L_START, flip=True)


def build_attack():
    f = ATTACK_START

    pose(f + 0)

    pose(f + 2,
         root=(0, 0.008, -0.002), pelvis=(0, 0, -4),
         torso=(-3, 0, -6), head=(0, 0, 4),
         arm_upper_r=(-32, 0, -8), arm_lower_r=(-16, 0, 0), hand_r=(4, 0, 0),
         arm_upper_l=(-8, 0, 6), arm_lower_l=(-14, 0, 0),
         leg_upper_r=(6, 0, 0), leg_lower_r=(6, 0, 0),
         leg_upper_l=(-3, 0, 0), leg_lower_l=(3, 0, 0))

    pose(f + 4,
         root=(0, 0.016, -0.004), pelvis=(0, 0, -7),
         torso=(-6, 0, -10), head=(-1, 0, 7),
         arm_upper_r=(-66, 0, -15), arm_lower_r=(-28, 0, 0), hand_r=(8, 0, 0),
         arm_upper_l=(-13, 0, 10), arm_lower_l=(-20, 0, 0),
         leg_upper_r=(9, 0, 0), leg_lower_r=(9, 0, 0),
         leg_upper_l=(-5, 0, 0), leg_lower_l=(4, 0, 0))

    pose(f + 6,
         root=(0, 0.022, -0.008), pelvis=(0, 0, -10),
         torso=(-8, 0, -13), head=(-2, 0, 10),
         arm_upper_r=(-100, 0, -22), arm_lower_r=(-38, 0, 0), hand_r=(12, 0, 0),
         arm_upper_l=(-17, 0, 13), arm_lower_l=(-26, 0, 0),
         leg_upper_r=(12, 0, 0), leg_lower_r=(11, 0, 0),
         leg_upper_l=(-6, 0, 0), leg_lower_l=(5, 0, 0))

    pose(f + 8,
         root=(0, 0.028, -0.011), pelvis=(0, 0, -12),
         torso=(-10, 0, -16), head=(-3, 0, 13),
         arm_upper_r=(-128, 0, -28), arm_lower_r=(-44, 0, 0), hand_r=(16, 0, 0),
         arm_upper_l=(-20, 0, 15), arm_lower_l=(-30, 0, 0),
         leg_upper_r=(14, 0, 0), leg_lower_r=(13, 0, 0),
         leg_upper_l=(-7, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 10,
         root=(0, 0.032, -0.013), pelvis=(0, 0, -14),
         torso=(-11, 0, -18), head=(-4, 0, 15),
         arm_upper_r=(-146, 0, -31), arm_lower_r=(-47, 0, 0), hand_r=(19, 0, 0),
         arm_upper_l=(-22, 0, 16), arm_lower_l=(-32, 0, 0),
         leg_upper_r=(15, 0, 0), leg_lower_r=(14, 0, 0),
         leg_upper_l=(-8, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 12,
         root=(0, 0.034, -0.014), pelvis=(0, 0, -15),
         torso=(-12, 0, -19), head=(-4, 0, 16),
         arm_upper_r=(-154, 0, -32), arm_lower_r=(-48, 0, 0), hand_r=(20, 0, 0),
         arm_upper_l=(-23, 0, 17), arm_lower_l=(-33, 0, 0),
         leg_upper_r=(15, 0, 0), leg_lower_r=(14, 0, 0),
         leg_upper_l=(-8, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 14,
         root=(0, 0.034, -0.014), pelvis=(0, 0, -16),
         torso=(-13, 0, -20), head=(-4, 0, 17),
         arm_upper_r=(-158, 0, -33), arm_lower_r=(-50, 0, 0), hand_r=(22, 0, 0),
         arm_upper_l=(-23, 0, 17), arm_lower_l=(-33, 0, 0),
         leg_upper_r=(15, 0, 0), leg_lower_r=(14, 0, 0),
         leg_upper_l=(-8, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 15,
         root=(0, 0.028, -0.014), pelvis=(0, 0, -11),
         torso=(-9, 0, -14), head=(-3, 0, 12),
         arm_upper_r=(-146, 0, -28), arm_lower_r=(-44, 0, 0), hand_r=(20, 0, 0),
         arm_upper_l=(-19, 0, 13), arm_lower_l=(-30, 0, 0),
         leg_upper_r=(13, 0, 0), leg_lower_r=(13, 0, 0),
         leg_upper_l=(-7, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 16,
         root=(0, 0.016, -0.017), pelvis=(0, 0, -4),
         torso=(-3, 0, -5), head=(-1, 0, 5),
         arm_upper_r=(-124, 0, -18), arm_lower_r=(-35, 0, 0), hand_r=(18, 0, 0),
         arm_upper_l=(-12, 0, 7), arm_lower_l=(-25, 0, 0),
         leg_upper_r=(9, 0, 0), leg_lower_r=(11, 0, 0),
         leg_upper_l=(-4, 0, 0), leg_lower_l=(5, 0, 0))

    pose(f + 17,
         root=(0, -0.004, -0.020), pelvis=(0, 0, 3),
         torso=(4, 0, 4), head=(1, 0, -2),
         arm_upper_r=(-100, 0, -6), arm_lower_r=(-26, 0, 0), hand_r=(16, 0, 0),
         arm_upper_l=(-3, 0, 0), arm_lower_l=(-20, 0, 0),
         leg_upper_r=(4, 0, 0), leg_lower_r=(9, 0, 0),
         leg_upper_l=(0, 0, 0), leg_lower_l=(5, 0, 0))

    pose(f + 18,
         root=(0, -0.028, -0.023), pelvis=(0, 0, 9),
         torso=(10, 0, 12), head=(3, 0, -7),
         arm_upper_r=(-76, 0, 8), arm_lower_r=(-17, 0, 0), hand_r=(16, 0, 0),
         arm_upper_l=(7, 0, -7), arm_lower_l=(-16, 0, 0),
         leg_upper_r=(-3, 0, 0), leg_lower_r=(8, 0, 0),
         leg_upper_l=(5, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 19,
         root=(0, -0.052, -0.026), pelvis=(0, 0, 14),
         torso=(15, 0, 19), head=(5, 0, -10),
         arm_upper_r=(-55, 0, 20), arm_lower_r=(-10, 0, 0), hand_r=(18, 0, 0),
         arm_upper_l=(16, 0, -12), arm_lower_l=(-14, 0, 0),
         leg_upper_r=(-9, 0, 0), leg_lower_r=(8, 0, 0),
         leg_upper_l=(10, 0, 0), leg_lower_l=(7, 0, 0))

    pose(f + 20,
         root=(0, -0.058, -0.030), pelvis=(0, 0, 17),
         torso=(17, 0, 23), head=(5, 0, -11),
         arm_upper_r=(-44, 0, 34), arm_lower_r=(-20, 0, 0), hand_r=(24, 0, 0),
         arm_upper_l=(20, 0, -15), arm_lower_l=(-20, 0, 0),
         leg_upper_r=(-11, 0, 0), leg_lower_r=(9, 0, 0),
         leg_upper_l=(12, 0, 0), leg_lower_l=(8, 0, 0))

    pose(f + 22,
         root=(0, -0.050, -0.028), pelvis=(0, 0, 15),
         torso=(15, 0, 21), head=(4, 0, -9),
         arm_upper_r=(-34, 0, 46), arm_lower_r=(-34, 0, 0), hand_r=(28, 0, 0),
         arm_upper_l=(18, 0, -13), arm_lower_l=(-24, 0, 0),
         leg_upper_r=(-10, 0, 0), leg_lower_r=(9, 0, 0),
         leg_upper_l=(11, 0, 0), leg_lower_l=(8, 0, 0))

    pose(f + 24,
         root=(0, -0.038, -0.022), pelvis=(0, 0, 12),
         torso=(12, 0, 17), head=(3, 0, -7),
         arm_upper_r=(-27, 0, 42), arm_lower_r=(-32, 0, 0), hand_r=(22, 0, 0),
         arm_upper_l=(14, 0, -10), arm_lower_l=(-20, 0, 0),
         leg_upper_r=(-8, 0, 0), leg_lower_r=(7, 0, 0),
         leg_upper_l=(9, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 26,
         root=(0, -0.024, -0.014), pelvis=(0, 0, 8),
         torso=(8, 0, 12), head=(2, 0, -4),
         arm_upper_r=(-18, 0, 30), arm_lower_r=(-24, 0, 0), hand_r=(14, 0, 0),
         arm_upper_l=(9, 0, -6), arm_lower_l=(-14, 0, 0),
         leg_upper_r=(-5, 0, 0), leg_lower_r=(5, 0, 0),
         leg_upper_l=(6, 0, 0), leg_lower_l=(4, 0, 0))

    pose(f + 28,
         root=(0, -0.010, -0.006), pelvis=(0, 0, 3),
         torso=(3, 0, 5), head=(1, 0, -2),
         arm_upper_r=(-8, 0, 14), arm_lower_r=(-11, 0, 0), hand_r=(6, 0, 0),
         arm_upper_l=(4, 0, -2), arm_lower_l=(-6, 0, 0),
         leg_upper_r=(-2, 0, 0), leg_lower_r=(2, 0, 0),
         leg_upper_l=(2, 0, 0), leg_lower_l=(2, 0, 0))

    pose(f + 30)


def build_bump():
    """Walk into a wall: one step in, both hands brace against it, push off
    and step back. Reads as a checked step rather than a recoil, so the
    elbows have to fold while the hands stay put -- the arms only shorten by
    as much as the root actually travels after contact."""
    f = BUMP_START

    pose(f + 0)

    pose(f + 2,
         root=(0, -0.008, -0.004),
         torso=(3, 0, 0), head=(2, 0, 0),
         arm_upper_r=(-20, 0, -4), arm_lower_r=(-16, 0, 0), hand_r=(-8, 0, 0),
         arm_upper_l=(-20, 0, 4), arm_lower_l=(-16, 0, 0), hand_l=(-8, 0, 0),
         leg_upper_r=(-14, 0, 0), leg_lower_r=(18, 0, 0), foot_r=(-8, 0, 0),
         leg_upper_l=(7, 0, 0), leg_lower_l=(7, 0, 0))

    pose(f + 4,
         root=(0, -0.026, -0.010),
         torso=(7, 0, 0), head=(4, 0, 0),
         arm_upper_r=(-46, 0, -9), arm_lower_r=(-32, 0, 0), hand_r=(-24, 0, 0),
         arm_upper_l=(-46, 0, 9), arm_lower_l=(-32, 0, 0), hand_l=(-24, 0, 0),
         leg_upper_r=(-28, 0, 0), leg_lower_r=(16, 0, 0), foot_r=(-4, 0, 0),
         leg_upper_l=(13, 0, 0), leg_lower_l=(13, 0, 0), foot_l=(-8, 0, 0))

    pose(f + 6,
         root=(0, -0.048, -0.018),
         torso=(11, 0, 0), head=(6, 0, 0),
         arm_upper_r=(-66, 0, -12), arm_lower_r=(-36, 0, 0), hand_r=(-42, 0, 0),
         arm_upper_l=(-66, 0, 12), arm_lower_l=(-36, 0, 0), hand_l=(-42, 0, 0),
         leg_upper_r=(-34, 0, 0), leg_lower_r=(7, 0, 0), foot_r=(5, 0, 0),
         leg_upper_l=(17, 0, 0), leg_lower_l=(17, 0, 0), foot_l=(-14, 0, 0))

    pose(f + 7,
         root=(0, -0.062, -0.022),
         torso=(13, 0, 0), head=(7, 0, 0),
         arm_upper_r=(-72, 0, -13), arm_lower_r=(-36, 0, 0), hand_r=(-52, 0, 0),
         arm_upper_l=(-72, 0, 13), arm_lower_l=(-36, 0, 0), hand_l=(-52, 0, 0),
         leg_upper_r=(-35, 0, 0), leg_lower_r=(9, 0, 0), foot_r=(6, 0, 0),
         leg_upper_l=(18, 0, 0), leg_lower_l=(19, 0, 0), foot_l=(-16, 0, 0))

    pose(f + 8,
         root=(0, -0.086, -0.030),
         torso=(11, 0, 0), head=(0, 0, 0),
         arm_upper_r=(-68, 0, -22), arm_lower_r=(-48, 0, 0), hand_r=(-56, 0, 0),
         arm_upper_l=(-68, 0, 22), arm_lower_l=(-48, 0, 0), hand_l=(-56, 0, 0),
         leg_upper_r=(-32, 0, 0), leg_lower_r=(16, 0, 0), foot_r=(8, 0, 0),
         leg_upper_l=(19, 0, 0), leg_lower_l=(24, 0, 0), foot_l=(-18, 0, 0))

    pose(f + 9,
         root=(0, -0.100, -0.040),
         torso=(7, 0, 0), head=(-8, 0, 0),
         arm_upper_r=(-64, 0, -30), arm_lower_r=(-58, 0, 0), hand_r=(-58, 0, 0),
         arm_upper_l=(-64, 0, 30), arm_lower_l=(-58, 0, 0), hand_l=(-58, 0, 0),
         leg_upper_r=(-29, 0, 0), leg_lower_r=(23, 0, 0), foot_r=(10, 0, 0),
         leg_upper_l=(18, 0, 0), leg_lower_l=(27, 0, 0), foot_l=(-18, 0, 0))

    pose(f + 10,
         root=(0, -0.078, -0.036),
         torso=(2, 0, 0), head=(-10, 0, 0),
         arm_upper_r=(-66, 0, -24), arm_lower_r=(-40, 0, 0), hand_r=(-52, 0, 0),
         arm_upper_l=(-66, 0, 24), arm_lower_l=(-40, 0, 0), hand_l=(-52, 0, 0),
         leg_upper_r=(-27, 0, 0), leg_lower_r=(20, 0, 0), foot_r=(8, 0, 0),
         leg_upper_l=(15, 0, 0), leg_lower_l=(22, 0, 0), foot_l=(-14, 0, 0))

    pose(f + 12,
         root=(0, -0.022, -0.024),
         torso=(-4, 0, 0), head=(-8, 0, 0),
         arm_upper_r=(-44, 0, -14), arm_lower_r=(-28, 0, 0), hand_r=(-32, 0, 0),
         arm_upper_l=(-44, 0, 14), arm_lower_l=(-28, 0, 0), hand_l=(-32, 0, 0),
         leg_upper_r=(-14, 0, 0), leg_lower_r=(30, 0, 0), foot_r=(-8, 0, 0),
         leg_upper_l=(8, 0, 0), leg_lower_l=(12, 0, 0))

    pose(f + 14,
         root=(0, 0.014, -0.012),
         torso=(-5, 0, 0), head=(-4, 0, 0),
         arm_upper_r=(-20, 0, -6), arm_lower_r=(-16, 0, 0), hand_r=(-14, 0, 0),
         arm_upper_l=(-20, 0, 6), arm_lower_l=(-16, 0, 0), hand_l=(-14, 0, 0),
         leg_upper_r=(4, 0, 0), leg_lower_r=(14, 0, 0), foot_r=(-4, 0, 0),
         leg_upper_l=(-2, 0, 0), leg_lower_l=(6, 0, 0))

    pose(f + 16,
         root=(0, 0.006, -0.004),
         torso=(-2, 0, 0), head=(-1, 0, 0),
         arm_upper_r=(-7, 0, -2), arm_lower_r=(-6, 0, 0), hand_r=(-4, 0, 0),
         arm_upper_l=(-7, 0, 2), arm_lower_l=(-6, 0, 0), hand_l=(-4, 0, 0),
         leg_upper_r=(2, 0, 0), leg_lower_r=(5, 0, 0),
         leg_upper_l=(-1, 0, 0), leg_lower_l=(2, 0, 0))

    pose(f + 18)


ANIMATIONS = (
    ("idle", IDLE_START, IDLE_END, build_idle),
    ("walk_r", WALK_R_START, WALK_R_END, build_walk_r),
    ("walk_l", WALK_L_START, WALK_L_END, build_walk_l),
    ("attack", ATTACK_START, ATTACK_END, build_attack),
    ("bump", BUMP_START, BUMP_END, build_bump),
)


def apply(scene):
    missing = [n for n in ANIMATED if n not in OBJECTS]
    if missing:
        raise SystemExit(
            f"the scene has no object named {missing} -- animated segments "
            "are matched by name, so a rename or reparent has to be mirrored "
            "in ANIMATED")

    clear_animation(scene)

    scene.render.fps = FPS
    scene.render.fps_base = 1.0
    scene.frame_start = FRAME_START
    scene.frame_end = FRAME_END

    for name, start, _end, build in ANIMATIONS:
        build()
        scene.timeline_markers.new(name, frame=start)

    scene.frame_set(REST_FRAME)

    for name, start, end, _build in ANIMATIONS:
        print(f"  {name:8} frames {start}-{end}  {(end - start) / FPS:.3f}s")


def parse_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []
    save = False
    out = None
    for arg in argv:
        if arg == "--save":
            save = True
        elif out is None:
            out = arg
        else:
            raise SystemExit(f"unexpected argument: {arg}")
    return save, out


def main():
    save, out = parse_args()
    scene = bpy.context.scene

    bind_existing(scene, REST_FRAME)
    print(f"bound {len(OBJECTS)} mesh objects from "
          f"{bpy.data.filepath or '(unsaved)'}")
    apply(scene)

    if save or out:
        path = os.path.abspath(out or bpy.data.filepath)
        bpy.ops.wm.save_as_mainfile(filepath=path)
        print(f"saved {path}")
    else:
        print("dry run, pass --save to write the .blend")


if __name__ == "__main__":
    main()
