# File: assets3d/scripts/controls.py
#
# 3D versions of the UI controls in the SVG pack (assets/ui/README.md), same sizes, so they drop into
# the same slots in Source/ui:
#   button_off / button_hover / button_on   240x68 (@2x of 120x34)   brass plaque, screws at the ends,
#                                                                     dark glass window; hover = faint
#                                                                     amber glow, on = glowing red
#   lever_off / lever_on                    240x280 (@2x of 120x140) brass base plate, pivot boss, steel
#                                                                     rod, cracked bone ball
#   panel                                   2080x292 (@2x of 1040x146) quick-mix box: brass frame,
#                                                                     corner screws, dark worn inset
# Abandoned look to match the knob: tarnish, verdigris in the crevices, dust, cold moonlight from the
# side. Orthographic camera straight down, framed exactly on each part; transparent background.
#
#   blender -b --factory-startup -P assets3d/scripts/controls.py -- --out <dir> [--only button,lever,panel]
# Units: 1 Blender unit = 1 cm at 1x size (the plaque is 12 x 3.4).

import argparse
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib3d import *  # noqa: E402,F401,F403

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--out", default="assets3d/renders/controls")
ap.add_argument("--only", default="button,lever,panel")
ap.add_argument("--samples", type=int, default=64)
ap.add_argument("--exposure", type=float, default=0.0)
args = ap.parse_args(argv)
ONLY = set(x.strip() for x in args.only.split(","))

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

# ================================================================ materials
def mat_brass():
    m, nt, out = new_material("Brass")
    co = coords(nt)
    n1 = noise(nt, co, 1.5, 10.0, 0.6)
    brass = principled(nt, (0.32, 0.21, 0.08), 0.38, 1.0)
    link_in(nt, ramp(nt, n1, [(0.3, (0.14, 0.09, 0.03)), (0.55, (0.3, 0.2, 0.08)), (0.75, (0.4, 0.29, 0.12))]), brass, ["Base Color"])
    link_in(nt, ramp(nt, n1, [(0.3, gray(0.25)), (0.8, gray(0.55))]), brass, ["Roughness"])
    link_in(nt, bump(nt, noise(nt, co, 25.0, 8.0, 0.6), 0.15, 0.01), brass, ["Normal"])
    verd = principled(nt, (0.08, 0.17, 0.13), 0.85)
    patches = ramp(nt, noise(nt, co, 0.8, 8.0, 0.65), [(0.5, gray(0.0)), (0.6, gray(1.0))])
    crevice = ao_dirt(nt, 0.3)
    vmask = math_node(nt, "MINIMUM", math_node(nt, "ADD", math_node(nt, "MULTIPLY", crevice, 0.9),
                                               math_node(nt, "MULTIPLY", patches, 0.6)), 1.0)
    s = mix_shaders(nt, vmask, brass, verd)
    s = mix_shaders(nt, math_node(nt, "MULTIPLY", crevice, 0.5), s, principled(nt, (0.02, 0.016, 0.01), 0.9))
    finish(nt, out, dusty(nt, s, 0.4, 8.0))
    return m

def mat_window(state):
    """Dark glass in the plaque. off: black; hover: faint amber glow; on: glowing red."""
    m, nt, out = new_material(f"Window_{state}")
    co = coords(nt)
    glass = principled(nt, (0.01, 0.008, 0.008), 0.08)
    set_in(glass, ["Coat Weight", "Clearcoat"], 1.0)
    if state in ("on", "hover"):
        # glow from behind the glass: strongest in the middle, falling off towards the frame
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(coords(nt, "Generated"), sep.inputs[0])
        cx_ = math_node(nt, "SUBTRACT", 1.0, math_node(nt, "ABSOLUTE", math_node(nt, "MULTIPLY", math_node(nt, "SUBTRACT", sep.outputs["X"], 0.5), 2.0)))
        cy_ = math_node(nt, "SUBTRACT", 1.0, math_node(nt, "ABSOLUTE", math_node(nt, "MULTIPLY", math_node(nt, "SUBTRACT", sep.outputs["Y"], 0.5), 2.0)))
        falloff = math_node(nt, "POWER", math_node(nt, "MULTIPLY", math_node(nt, "POWER", cx_, 0.35), math_node(nt, "POWER", cy_, 0.8)), 1.5)
        if state == "on":
            set_in(glass, ["Base Color"], (0.12, 0.005, 0.004, 1.0))
            set_in(glass, ["Emission Color", "Emission"], (0.85, 0.025, 0.012, 1.0))
            peak = 0.75
        else:
            set_in(glass, ["Emission Color", "Emission"], (1.0, 0.5, 0.22, 1.0))
            peak = 0.07
        link_in(nt, math_node(nt, "MULTIPLY", falloff, peak), glass, ["Emission Strength"])
    smudge = ramp(nt, noise(nt, co, 2.0, 6.0, 0.6), [(0.4, gray(0.05)), (0.75, gray(0.35))])
    link_in(nt, smudge, glass, ["Coat Roughness", "Clearcoat Roughness"])
    finish(nt, out, dusty(nt, glass, 0.25, 12.0))
    return m

def mat_steel():
    m, nt, out = new_material("DullSteel")
    co = coords(nt)
    b = principled(nt, (0.3, 0.3, 0.29), 0.4, 1.0)
    link_in(nt, ramp(nt, noise(nt, co, 4.0, 8.0, 0.6), [(0.3, gray(0.3)), (0.75, gray(0.6))]), b, ["Roughness"])
    rust = principled(nt, (0.08, 0.035, 0.015), 0.9)
    spots = ramp(nt, noise(nt, co, 3.0, 10.0, 0.7), [(0.6, gray(0.0)), (0.7, gray(1.0))])
    finish(nt, out, dusty(nt, mix_shaders(nt, spots, b, rust), 0.3, 10.0))
    return m

def mat_bone():
    m, nt, out = new_material("Bone")
    co = coords(nt)
    b = principled(nt, (0.58, 0.49, 0.32), 0.55)
    link_in(nt, ramp(nt, noise(nt, co, 3.0, 10.0, 0.6), [(0.3, (0.42, 0.34, 0.2)), (0.55, (0.58, 0.49, 0.32)), (0.8, (0.66, 0.58, 0.42))]), b, ["Base Color"])
    set_in(b, ["Subsurface Weight", "Subsurface"], 0.1)
    s = mix_shaders(nt, math_node(nt, "MINIMUM", math_node(nt, "MULTIPLY", ao_dirt(nt, 0.3), 1.2), 1.0), b,
                    principled(nt, (0.06, 0.045, 0.025), 0.85))
    crack = math_node(nt, "MULTIPLY", ramp(nt, voronoi_edges(nt, co, 2.2), [(0.0, gray(1.0)), (0.02, gray(0.0))]),
                      ramp(nt, noise(nt, co, 1.0, 4.0, 0.5), [(0.45, gray(0.0)), (0.55, gray(1.0))]))
    finish(nt, out, mix_shaders(nt, crack, s, principled(nt, (0.025, 0.018, 0.01), 0.9)))
    return m

def mat_inset():
    """Dark, worn lacquered wood for the inside of the quick-mix panel."""
    m, nt, out = new_material("PanelInset")
    co = coords(nt)
    grain = noise(nt, mapping(nt, co, scale=(0.08, 1.2, 1.0)), 3.0, 10.0, 0.6)
    b = principled(nt, (0.02, 0.014, 0.01), 0.35)
    link_in(nt, ramp(nt, grain, [(0.3, (0.012, 0.008, 0.006)), (0.7, (0.03, 0.02, 0.014))]), b, ["Base Color"])
    set_in(b, ["Coat Weight", "Clearcoat"], 0.5)
    link_in(nt, ramp(nt, noise(nt, co, 0.6, 6.0, 0.6), [(0.35, gray(0.1)), (0.7, gray(0.5))]), b, ["Coat Roughness", "Clearcoat Roughness"])
    link_in(nt, bump(nt, grain, 0.2, 0.02), b, ["Normal"])
    worn = ramp(nt, noise(nt, co, 0.35, 8.0, 0.65), [(0.58, gray(0.0)), (0.68, gray(1.0))])
    s = mix_shaders(nt, worn, b, principled(nt, (0.05, 0.035, 0.022), 0.8))     # rubbed-through spots
    finish(nt, out, dusty(nt, s, 0.45, 6.0))
    return m

def mat_simple(name, color, rough, metal=0.0):
    m, nt, out = new_material(name)
    finish(nt, out, principled(nt, color, rough, metal))
    return m

brass, steel, bone, inset = mat_brass(), mat_steel(), mat_bone(), mat_inset()
slot_dark = mat_simple("SlotDark", (0.004, 0.003, 0.003), 0.9)

# ================================================================ parts
def screw(name, x, y, z, r=0.32, angle=0.6):
    head = cylinder(name, r, 0.16, (x, y, z + 0.08), 32, steel)
    add_bevel(head, r * 0.45, 4, 40)
    smooth(head, 40)
    slot = box(name + "_slot", (r * 1.8, r * 0.28, 0.06), (0, 0, 0), slot_dark)
    slot.location = (x, y, z + 0.17)
    slot.rotation_euler = (0, 0, angle)
    return [head, slot]

def plaque(cx, state):
    """Brass plaque 12 x 3.4: raised border with screw ends around a recessed glass window."""
    W, H, T, END, SIDE = 12.0, 3.4, 0.35, 1.45, 0.48
    parts = []
    def bar(n, x0, y0, x1, y1, h):
        o = box_minmax(n, (cx + x0, y0, 0.0), (cx + x1, y1, h), brass)
        add_bevel(o, 0.09, 3, 40)
        smooth(o, 40)
        parts.append(o)
    bar("PlqL", -W / 2, -H / 2, -W / 2 + END, H / 2, T)
    bar("PlqR", W / 2 - END, -H / 2, W / 2, H / 2, T)
    bar("PlqT", -W / 2 + END - 0.05, H / 2 - SIDE, W / 2 - END + 0.05, H / 2, T)
    bar("PlqB", -W / 2 + END - 0.05, -H / 2, W / 2 - END + 0.05, -H / 2 + SIDE, T)
    win = box_minmax("Window", (cx - W / 2 + END, -H / 2 + SIDE, 0.0), (cx + W / 2 - END, H / 2 - SIDE, T * 0.45), mat_window(state))
    parts.append(win)
    for k, sx in enumerate((-1, 1)):
        parts += screw(f"PlqScrew{k}", cx + sx * (W / 2 - END / 2), 0.0, T, 0.34, 0.5 + k * 1.1)
    return parts, (cx, 0.0, W, H)

def lever(cx, on):
    """Frame 12 x 14: base plate low in the frame, rod rising from the pivot to a bone ball."""
    parts = []
    base = box_minmax("LevBase", (cx - 4.2, -6.6, 0.0), (cx + 4.2, -2.0, 0.5), brass)
    add_bevel(base, 0.12, 3, 40)
    smooth(base, 40)
    parts.append(base)
    parts.append(box_minmax("LevSlot", (cx - 2.4, -4.65, 0.4), (cx + 2.4, -3.95, 0.56), slot_dark))
    for k, (sx, sy) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
        parts += screw(f"LevScrew{k}", cx + sx * 3.5, -4.3 + sy * 1.65, 0.5, 0.3, 0.3 + k * 0.7)
    boss = cylinder("LevBoss", 0.75, 0.5, (cx, -4.3, 0.75), 48, steel)
    add_bevel(boss, 0.2, 4, 40)
    smooth(boss, 40)
    parts.append(boss)
    ang = math.radians(-24 if on else 24)          # +z rotation is counter-clockwise: off leans left
    d = Vector((-math.sin(ang), math.cos(ang), 0.0))
    p0 = Vector((cx, -4.3, 1.0))
    tip = p0 + d * 8.4 + Vector((0, 0, 1.6))
    parts.append(tube("LevRod", [tuple(p0), tuple(p0.lerp(tip, 0.5) + Vector((0, 0, 0.25))), tuple(tip)], [0.42, 0.34, 0.3], steel, order=3))
    parts.append(ellipsoid("LevBall", (1.15, 1.15, 1.1), tuple(tip + d * 0.6 + Vector((0, 0, 0.25))), bone, 48, 24))
    return parts, (cx, 0.0, 12.0, 14.0)

def panel(cx):
    """Quick-mix box 104 x 14.6: bevelled brass frame, corner screws, dark worn inset."""
    W, H, F = 104.0, 14.6, 0.9
    parts = [box_minmax("PanInset", (cx - W / 2 + F, -H / 2 + F, 0.0), (cx + W / 2 - F, H / 2 - F, 0.25), inset)]
    for n, (x0, y0, x1, y1) in (("PanT", (-W / 2, H / 2 - F, W / 2, H / 2)), ("PanB", (-W / 2, -H / 2, W / 2, -H / 2 + F)),
                                ("PanL", (-W / 2, -H / 2, -W / 2 + F, H / 2)), ("PanR", (W / 2 - F, -H / 2, W / 2, H / 2))):
        o = box_minmax(n, (cx + x0, y0, 0.0), (cx + x1, y1, 0.55), brass)
        add_bevel(o, 0.14, 3, 40)
        smooth(o, 40)
        parts.append(o)
    for k, (sx, sy) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
        parts += screw(f"PanScrew{k}", cx + sx * (W / 2 - F / 2), sy * (H / 2 - F / 2), 0.55, 0.3, 0.4 + k * 0.9)
    return parts, (cx, 0.0, W, H)

# ================================================================ light (shared; parts are laid out far apart on x)
def lights_for(cx, w):
    """Warm key from the top-left, cold moonlight grazing from the right, soft highlight from above."""
    k = max(w / 12.0, 1.0)
    made = [area("Key", (cx - 9 * k, 7 * k, 18 * k), (cx, 0, 0), 7 * k, 1600 * k * k, (1.0, 0.8, 0.6)),
            spot("Moon", (cx + 22 * k, -3 * k, 3.5 * k), (cx, 0, 0.3), 9000 * k * k, (0.5, 0.65, 1.0), 25, 0.7, 0.2 * k),
            area("Soft", (cx - 3 * k, 3 * k, 14 * k), (cx, 0, 0), 3 * k, 160 * k * k, (1.0, 0.88, 0.75))]
    return made

world = bpy.data.worlds.new("Dark")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
set_in(bg, ["Color"], (0.004, 0.005, 0.008, 1.0))

cam_data = bpy.data.cameras.new("Cam")
cam_data.type = "ORTHO"
cam_data.sensor_fit = "HORIZONTAL"
cam = link(bpy.data.objects.new("Cam", cam_data))
scene.camera = cam

jobs = []        # (name, parts, frame, lights, resolution)
x = 0.0
if "button" in ONLY:
    for state in ("off", "hover", "on"):
        parts, frame = plaque(x, state)
        jobs.append((f"button_{state}", parts, frame, lights_for(x, 12.0), (240, 68)))
        x += 200.0
if "lever" in ONLY:
    for on in (False, True):
        parts, frame = lever(x, on)
        jobs.append((f"lever_{'on' if on else 'off'}", parts, frame, lights_for(x, 12.0), (240, 280)))
        x += 200.0
if "panel" in ONLY:
    parts, frame = panel(x + 300.0)
    jobs.append(("panel", parts, frame, lights_for(x + 300.0, 104.0), (2080, 292)))

all_parts = [o for j in jobs for o in j[1]]
all_lights = [o for j in jobs for o in j[3]]
scene.view_settings.exposure = args.exposure

for name, parts, (fx, fy, fw, fh), lts, (rx, ry) in jobs:
    keep = set(o.name for o in parts + lts)
    for o in all_parts + all_lights:
        o.hide_render = o.name not in keep
    cam.location = (fx, fy, 60.0)
    cam.rotation_euler = (0.0, 0.0, 0.0)
    cam_data.ortho_scale = fw                       # frame exactly the part's width (height from aspect)
    setup_cycles(args.samples, rx, ry, transparent=True)
    scene.view_settings.exposure = args.exposure
    render_to(os.path.join(args.out, f"{name}@2x.png"))
