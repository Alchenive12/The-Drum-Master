# File: assets3d/scripts/stage.py
#
# The main stage for The Drum Master (docs/04-ui-concept.md, "The room and the very tall master"):
# - an eerie late-night room: faded striped wallpaper with damp stains, floorboards, a tall cracked
#   window high on the left with a shaft of cold moonlight and dust, an old dusty drum set in the
#   dark back corner (split bass-drum head, floor tom under a dust sheet, tarnished cymbals);
# - the master: very tall, impossibly thin legs bowed outwards with knobbly knees, charcoal
#   pinstripe trousers, long curled pointed shoes; long bony hands with inhumanly elongated fingers
#   draped over the wooden control bar; only the bottom of his cracked porcelain mask (stitched
#   smile) shows at the top edge. Original design: no rosy cheeks, no tear streaks, no striped cuffs.
#
# Strings, the Shadow Play ring and all controls are drawn by the plugin, not here.
#
#   blender -b --factory-startup -P assets3d/scripts/stage.py -- --out <dir> [--layers preview,...]
# Layers: preview (everything), background (room + legs), mask, hands (bar + hands + sleeves).
# Units: metres. Frame: 16:10, matching the plugin window.

import argparse
import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib3d import *  # noqa: E402,F401,F403

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--out", default="assets3d/renders/stage")
ap.add_argument("--layers", default="preview")
ap.add_argument("--width", type=int, default=1100)
ap.add_argument("--samples", type=int, default=96)
ap.add_argument("--lamp", type=float, default=1.0,
                help="warm lamp level, 1 = full; lower values are frames for the flicker effect")
args = ap.parse_args(argv)
WIDTH = args.width
HEIGHT = round(WIDTH * 10 / 16)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
random.seed(7)

# ================================================================ materials
def mat_wallpaper(axis):
    m, nt, out = new_material(f"Wallpaper_{axis}")
    co = coords(nt)
    # faded stripes: wide grey-blue band, narrow darker band, thin pale pinstripe
    stripes = wave_bands(nt, co, 2.6, axis)
    col = ramp(nt, stripes, [(0.0, (0.13, 0.145, 0.16)), (0.55, (0.085, 0.095, 0.11)),
                             (0.9, (0.19, 0.195, 0.19)), (0.94, (0.13, 0.145, 0.16))], "CONSTANT")
    # yellow-brown damp stains running down from the top, and darker damp near the floor
    streak_co = mapping(nt, co, scale=(2.2, 2.2, 0.35) if axis == "X" else (2.2, 2.2, 0.35))
    stain = ramp(nt, noise(nt, streak_co, 1.4, 8.0, 0.62), [(0.48, gray(0.0)), (0.66, gray(1.0))])
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(co, sep.inputs[0])
    low = ramp(nt, sep.outputs["Z"], [(0.0, gray(1.0)), (0.9, gray(0.0))])
    col = mix_color(nt, math_node(nt, "MULTIPLY", stain, 0.85), col, (0.07, 0.055, 0.03))
    col = mix_color(nt, math_node(nt, "MULTIPLY", low, 0.7), col, (0.02, 0.02, 0.018))
    paper = principled(nt, (0, 0, 0), 0.85)
    link_in(nt, col, paper, ["Base Color"])
    n = bump(nt, noise(nt, co, 18.0, 10.0, 0.7), 0.15, 0.01)
    link_in(nt, n, paper, ["Normal"])
    finish(nt, out, paper)
    return m

def mat_floor():
    m, nt, out = new_material("Floorboards")
    co = mapping(nt, coords(nt), rot=(0, 0, math.radians(90)))
    br = nt.nodes.new("ShaderNodeTexBrick")
    br.offset = 0.37
    set_in(br, ["Color1"], (0.09, 0.06, 0.037, 1))
    set_in(br, ["Color2"], (0.06, 0.041, 0.027, 1))
    set_in(br, ["Mortar"], (0.004, 0.003, 0.002, 1))
    set_in(br, ["Scale"], 1.8)
    set_in(br, ["Mortar Size"], 0.014)
    set_in(br, ["Bias"], 0.0)
    set_in(br, ["Brick Width"], 4.5)
    set_in(br, ["Row Height"], 0.25)
    nt.links.new(co, br.inputs["Vector"])
    grain_co = mapping(nt, co, scale=(0.25, 9.0, 1.0))
    grain = ramp(nt, noise(nt, grain_co, 6.0, 12.0, 0.65), [(0.3, gray(0.55)), (0.7, gray(1.15))])
    col = mix_color(nt, 1.0, br.outputs["Color"], grain, "MULTIPLY")
    wood = principled(nt, (0, 0, 0), 0.55)
    link_in(nt, col, wood, ["Base Color"])
    rough = ramp(nt, noise(nt, coords(nt), 2.0, 6.0, 0.5), [(0.3, gray(0.4)), (0.7, gray(0.8))])
    link_in(nt, rough, wood, ["Roughness"])
    gaps = math_node(nt, "SUBTRACT", 1.0, br.outputs["Fac"])
    grain_h = noise(nt, grain_co, 6.0, 12.0, 0.65)
    n = bump(nt, math_node(nt, "ADD", gaps, math_node(nt, "MULTIPLY", grain_h, 0.3)), 0.8, 0.02)
    link_in(nt, n, wood, ["Normal"])
    finish(nt, out, dusty(nt, wood, 0.35, 12.0))
    return m

def mat_plaster():
    m, nt, out = new_material("Ceiling")
    co = coords(nt)
    b = principled(nt, (0.05, 0.052, 0.055), 0.9)
    col = ramp(nt, noise(nt, co, 0.8, 8.0, 0.6), [(0.35, (0.02, 0.019, 0.017)), (0.65, (0.06, 0.062, 0.065))])
    link_in(nt, col, b, ["Base Color"])
    finish(nt, out, b)
    return m

def mat_old_wood(name="OldWood", base=(0.06, 0.035, 0.02)):
    m, nt, out = new_material(name)
    co = coords(nt)
    grain = noise(nt, mapping(nt, co, scale=(0.4, 0.4, 14.0)), 8.0, 10.0, 0.6)
    col = ramp(nt, grain, [(0.3, tuple(c * 0.5 for c in base)), (0.7, base)])
    b = principled(nt, (0, 0, 0), 0.6)
    link_in(nt, col, b, ["Base Color"])
    link_in(nt, bump(nt, grain, 0.2, 0.01), b, ["Normal"])
    finish(nt, out, dusty(nt, b, 0.5, 20.0))
    return m

def mat_glass():
    """Dirty cracked glass that lets light through (shadow rays see it as transparent)."""
    m, nt, out = new_material("CrackedGlass")
    co = coords(nt)
    glass = nt.nodes.new("ShaderNodeBsdfGlass")
    set_in(glass, ["Color"], (0.75, 0.8, 0.78, 1))
    set_in(glass, ["Roughness"], 0.08)
    set_in(glass, ["IOR"], 1.45)
    grime = principled(nt, (0.09, 0.09, 0.08), 0.9)
    crack = ramp(nt, voronoi_edges(nt, co, 3.0), [(0.0, gray(1.0)), (0.012, gray(0.0))])
    smear = ramp(nt, noise(nt, co, 3.0, 6.0, 0.6), [(0.5, gray(0.0)), (0.8, gray(0.6))])
    surf = mix_shaders(nt, math_node(nt, "MAXIMUM", crack, smear), glass, grime)
    lp = nt.nodes.new("ShaderNodeLightPath")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    final = mix_shaders(nt, lp.outputs["Is Shadow Ray"], surf, tr)
    finish(nt, out, final)
    return m

def mat_trousers():
    m, nt, out = new_material("PinstripeTrousers")
    co = coords(nt)
    stripes = wave_bands(nt, co, 22.0, "X")
    col = ramp(nt, stripes, [(0.0, (0.012, 0.012, 0.014)), (0.88, (0.012, 0.012, 0.014)),
                             (0.92, (0.17, 0.17, 0.175)), (0.95, (0.012, 0.012, 0.014))], "CONSTANT")
    worn = ramp(nt, noise(nt, co, 6.0, 8.0, 0.6), [(0.4, gray(0.0)), (0.75, gray(0.5))])
    col = mix_color(nt, worn, col, (0.05, 0.048, 0.045))
    b = principled(nt, (0, 0, 0), 0.8)
    link_in(nt, col, b, ["Base Color"])
    set_in(b, ["Sheen Weight", "Sheen"], 0.6)
    creases = noise(nt, mapping(nt, co, scale=(1.5, 1.5, 14.0)), 3.0, 4.0, 0.5)   # horizontal folds
    weave = noise(nt, co, 60.0, 4.0, 0.5)
    link_in(nt, bump(nt, math_node(nt, "ADD", math_node(nt, "MULTIPLY", creases, 0.8), math_node(nt, "MULTIPLY", weave, 0.2)), 0.35, 0.01), b, ["Normal"])
    finish(nt, out, dusty(nt, b, 0.35, 25.0))
    return m

def mat_leather():
    m, nt, out = new_material("OldLeather")
    co = coords(nt)
    b = principled(nt, (0.012, 0.01, 0.009), 0.3)
    set_in(b, ["Coat Weight", "Clearcoat"], 0.4)
    scuff = ramp(nt, noise(nt, co, 12.0, 8.0, 0.65), [(0.45, gray(0.25)), (0.7, gray(0.65))])
    link_in(nt, scuff, b, ["Roughness"])
    finish(nt, out, dusty(nt, b, 0.5, 30.0))
    return m

def mat_skin():
    m, nt, out = new_material("BonySkin")
    co = coords(nt)
    blot = noise(nt, co, 9.0, 10.0, 0.6)
    col = ramp(nt, blot, [(0.3, (0.4, 0.4, 0.39)), (0.6, (0.58, 0.58, 0.56)), (0.85, (0.66, 0.66, 0.64))])
    b = principled(nt, (0, 0, 0), 0.5)
    link_in(nt, col, b, ["Base Color"])
    set_in(b, ["Subsurface Weight", "Subsurface"], 0.15)
    set_in(b, ["Subsurface Radius"], (0.01, 0.006, 0.004))
    link_in(nt, bump(nt, noise(nt, co, 80.0, 6.0, 0.6), 0.25, 0.002), b, ["Normal"])
    dirt = principled(nt, (0.06, 0.05, 0.04), 0.8)
    finish(nt, out, mix_shaders(nt, math_node(nt, "MULTIPLY", ao_dirt(nt, 0.05, 1.2), 1.0, clamp=True), b, dirt))
    return m

def mat_porcelain():
    m, nt, out = new_material("CrackedPorcelain")
    co = coords(nt)
    col = ramp(nt, noise(nt, co, 5.0, 10.0, 0.6), [(0.3, (0.42, 0.38, 0.3)), (0.6, (0.6, 0.56, 0.47)), (0.85, (0.67, 0.63, 0.54))])
    b = principled(nt, (0, 0, 0), 0.45)
    link_in(nt, col, b, ["Base Color"])
    set_in(b, ["Coat Weight", "Clearcoat"], 0.15)
    set_in(b, ["Subsurface Weight", "Subsurface"], 0.05)
    edges = voronoi_edges(nt, mapping(nt, co, scale=(1.0, 1.0, 1.6)), 3.2)
    crack_line = ramp(nt, edges, [(0.0, gray(1.0)), (0.016, gray(0.0))])
    where = ramp(nt, noise(nt, co, 2.0, 4.0, 0.5), [(0.52, gray(0.0)), (0.6, gray(1.0))])
    crack = math_node(nt, "MULTIPLY", crack_line, where)
    link_in(nt, bump(nt, math_node(nt, "SUBTRACT", 1.0, crack), 0.6, 0.004), b, ["Normal"])
    dark = principled(nt, (0.02, 0.016, 0.012), 0.9)
    s = mix_shaders(nt, crack, b, dark)
    s = mix_shaders(nt, math_node(nt, "MULTIPLY", ao_dirt(nt, 0.05), 0.8), s, dark)
    # grime streaks running down (vertical stretched noise), heavier on the lower face
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(co, sep.inputs[0])
    drip = ramp(nt, noise(nt, mapping(nt, co, scale=(9.0, 9.0, 0.6)), 3.0, 6.0, 0.6), [(0.5, gray(0.0)), (0.72, gray(0.75))])
    grime = principled(nt, (0.12, 0.1, 0.075), 0.7)
    s = mix_shaders(nt, drip, s, grime)
    stains = ramp(nt, noise(nt, co, 1.6, 8.0, 0.65), [(0.55, gray(0.0)), (0.75, gray(0.8))])
    s = mix_shaders(nt, stains, s, principled(nt, (0.17, 0.12, 0.07), 0.8))   # old brown stains
    finish(nt, out, s)
    return m

def mat_simple(name, color, rough, metal=0.0, coat=0.0):
    m, nt, out = new_material(name)
    b = principled(nt, color, rough, metal)
    set_in(b, ["Coat Weight", "Clearcoat"], coat)
    finish(nt, out, b)
    return m

def mat_tarnished_bronze():
    m, nt, out = new_material("TarnishedBronze")
    co = coords(nt)
    b = principled(nt, (0.3, 0.2, 0.08), 0.4, 1.0)
    col = ramp(nt, noise(nt, co, 6.0, 10.0, 0.6), [(0.3, (0.12, 0.08, 0.03)), (0.7, (0.32, 0.22, 0.09))])
    link_in(nt, col, b, ["Base Color"])
    verd = principled(nt, (0.08, 0.15, 0.12), 0.85)
    patches = ramp(nt, noise(nt, co, 3.0, 8.0, 0.65), [(0.55, gray(0.0)), (0.66, gray(1.0))])
    finish(nt, out, dusty(nt, mix_shaders(nt, patches, b, verd), 0.7, 25.0))
    return m

def mat_drum_wrap():
    m, nt, out = new_material("DrumWrap")
    co = coords(nt)
    b = principled(nt, (0.09, 0.02, 0.02), 0.35)
    set_in(b, ["Coat Weight", "Clearcoat"], 0.5)
    fade = ramp(nt, noise(nt, co, 3.0, 8.0, 0.6), [(0.3, gray(0.0)), (0.8, gray(0.7))])
    faded = principled(nt, (0.07, 0.05, 0.045), 0.7)
    finish(nt, out, dusty(nt, mix_shaders(nt, fade, b, faded), 0.8, 20.0))
    return m

def mat_drum_head(split=False):
    m, nt, out = new_material("DrumHead_split" if split else "DrumHead")
    co = coords(nt)
    b = principled(nt, (0.22, 0.2, 0.165), 0.7)
    stain = ramp(nt, noise(nt, co, 4.0, 8.0, 0.6), [(0.4, gray(0.0)), (0.75, gray(0.8))])
    dirty = principled(nt, (0.16, 0.14, 0.1), 0.85)
    s = mix_shaders(nt, stain, b, dirty)
    if split:
        # one long tear across the head, slightly wavy
        tear_co = mapping(nt, co, scale=(1.0, 1.0, 1.0), rot=(0, math.radians(35), 0))
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tear_co, sep.inputs[0])
        wobble = math_node(nt, "MULTIPLY", noise(nt, co, 8.0, 4.0, 0.6), 0.05)
        line = math_node(nt, "ABSOLUTE", math_node(nt, "SUBTRACT", sep.outputs["X"], wobble))
        tear = ramp(nt, line, [(0.0, gray(1.0)), (0.012, gray(0.0))])
        s = mix_shaders(nt, tear, s, principled(nt, (0.0, 0.0, 0.0), 1.0))
    finish(nt, out, dusty(nt, s, 0.8, 25.0))
    return m

def mat_dust_sheet():
    m, nt, out = new_material("DustSheet")
    co = coords(nt)
    b = principled(nt, (0.32, 0.31, 0.28), 0.95)
    set_in(b, ["Sheen Weight", "Sheen"], 0.5)
    stain = ramp(nt, noise(nt, co, 3.0, 8.0, 0.6), [(0.45, gray(0.0)), (0.8, gray(0.7))])
    dirty = principled(nt, (0.12, 0.11, 0.09), 0.95)
    finish(nt, out, mix_shaders(nt, stain, b, dirty))
    return m

def mat_volume():
    m, nt, out = new_material("RoomHaze")
    v = nt.nodes.new("ShaderNodeVolumePrincipled")
    set_in(v, ["Density"], 0.028)
    set_in(v, ["Anisotropy"], 0.45)
    set_in(v, ["Color"], (0.8, 0.85, 1.0, 1))
    nt.links.new(v.outputs[0], out.inputs["Volume"])
    return m

# ================================================================ camera
cam_data = bpy.data.cameras.new("Cam")
cam_data.lens = 24.0
cam_data.sensor_width = 36.0
cam_data.sensor_fit = "HORIZONTAL"
cam = link(bpy.data.objects.new("Cam", cam_data))
cam.location = (0.0, -4.2, 1.55)
look_at(cam, (0.0, 3.5, 1.05))
scene.camera = cam
scene.render.resolution_x, scene.render.resolution_y = WIDTH, HEIGHT
bpy.context.view_layer.update()

# ================================================================ room
X0, X1, Y0, Y1, H, T = -4.5, 4.5, -4.6, 6.0, 5.4, 0.2
WIN_Y0, WIN_Y1, WIN_Z0, WIN_Z1 = 0.9, 2.5, 1.7, 4.0

room = []          # background layer objects
wp_x, wp_y = mat_wallpaper("X"), mat_wallpaper("Y")
room.append(box_minmax("Floor", (X0 - T, Y0 - T, -0.1), (X1 + T, Y1 + T, 0.0), mat_floor()))
room.append(box_minmax("Ceiling", (X0 - T, Y0 - T, H), (X1 + T, Y1 + T, H + T), mat_plaster()))
room.append(box_minmax("BackWall", (X0 - T, Y1, 0.0), (X1 + T, Y1 + T, H), wp_x))
room.append(box_minmax("FrontWall", (X0 - T, Y0 - T, 0.0), (X1 + T, Y0, H), wp_x))
room.append(box_minmax("RightWall", (X1, Y0, 0.0), (X1 + T, Y1, H), wp_y))
# left wall in four pieces around the window opening
room.append(box_minmax("LeftWall_low", (X0 - T, Y0, 0.0), (X0, Y1, WIN_Z0), wp_y))
room.append(box_minmax("LeftWall_high", (X0 - T, Y0, WIN_Z1), (X0, Y1, H), wp_y))
room.append(box_minmax("LeftWall_front", (X0 - T, Y0, WIN_Z0), (X0, WIN_Y0, WIN_Z1), wp_y))
room.append(box_minmax("LeftWall_back", (X0 - T, WIN_Y1, WIN_Z0), (X0, Y1, WIN_Z1), wp_y))

skirting = mat_old_wood("Skirting", (0.03, 0.02, 0.014))
room.append(box_minmax("Skirt_back", (X0, Y1 - 0.03, 0.0), (X1, Y1, 0.16), skirting))
room.append(box_minmax("Skirt_right", (X1 - 0.03, Y0, 0.0), (X1, Y1, 0.16), skirting))
room.append(box_minmax("Skirt_left", (X0, Y0, 0.0), (X0 + 0.03, Y1, 0.16), skirting))

# window: frame, mullions, cracked panes (one pane missing)
frame = mat_old_wood("WindowFrame", (0.035, 0.03, 0.026))
fx0, fx1 = X0 - 0.12, X0 + 0.04
room.append(box_minmax("Sill", (fx0, WIN_Y0 - 0.08, WIN_Z0 - 0.06), (X0 + 0.14, WIN_Y1 + 0.08, WIN_Z0), frame))
room.append(box_minmax("Head", (fx0, WIN_Y0 - 0.06, WIN_Z1), (fx1, WIN_Y1 + 0.06, WIN_Z1 + 0.08), frame))
room.append(box_minmax("Jamb_f", (fx0, WIN_Y0 - 0.06, WIN_Z0), (fx1, WIN_Y0, WIN_Z1), frame))
room.append(box_minmax("Jamb_b", (fx0, WIN_Y1, WIN_Z0), (fx1, WIN_Y1 + 0.06, WIN_Z1), frame))
ym = (WIN_Y0 + WIN_Y1) / 2.0
room.append(box_minmax("Mullion_v", (fx0 + 0.04, ym - 0.025, WIN_Z0), (fx1 - 0.04, ym + 0.025, WIN_Z1), frame))
z_rows = [WIN_Z0 + (WIN_Z1 - WIN_Z0) * k / 3.0 for k in range(4)]
for k in (1, 2):
    room.append(box_minmax(f"Mullion_h{k}", (fx0 + 0.04, WIN_Y0, z_rows[k] - 0.025), (fx1 - 0.04, WIN_Y1, z_rows[k] + 0.025), frame))
glass = mat_glass()
for col_i, (ya, yb) in enumerate(((WIN_Y0, ym), (ym, WIN_Y1))):
    for row in range(3):
        if col_i == 1 and row == 2:
            continue                                   # broken-out pane
        room.append(box_minmax(f"Pane_{col_i}{row}", (X0 - 0.045, ya, z_rows[row]), (X0 - 0.035, yb, z_rows[row + 1]), glass))

# ---------------------------------------------------------------- old drum set, back right corner
wrap, head, split_head = mat_drum_wrap(), mat_drum_head(), mat_drum_head(split=True)
bronze, chrome = mat_tarnished_bronze(), mat_simple("DullChrome", (0.25, 0.25, 0.24), 0.45, 1.0)
kit = []
# bass drum on its side, front head (towards the camera) split
bd = cylinder("BassShell", 0.3, 0.42, (0, 0, 0), 64, wrap)
bd.rotation_euler = (math.radians(90), 0, math.radians(-12))
bd.location = (2.4, 3.65, 0.31)
kit.append(bd)
bdh = cylinder("BassHead", 0.29, 0.01, (0, 0, 0.215), 64, split_head)   # faces the camera
bdh.parent = bd
kit.append(bdh)
for k, z in enumerate((-0.2, 0.2)):
    hoop = tube(f"BassHoop{k}", [(math.cos(a) * 0.305, math.sin(a) * 0.305, z) for a in [i * math.pi / 16 for i in range(33)]],
                [0.018] * 33, chrome, order=3, caps=False)
    hoop.parent = bd
    kit.append(hoop)
# rack tom on the bass drum, tilted towards the camera, and a snare on a stand
def drum(name, radius, depth, loc, tilt_deg, heads=True):
    shell = cylinder(name, radius, depth, (0, 0, 0), 48, wrap)
    shell.location, shell.rotation_euler = loc, (math.radians(tilt_deg), 0, 0)
    parts = [shell]
    if heads:
        top = cylinder(name + "_head", radius * 0.98, 0.006, (0, 0, depth / 2 + 0.003), 48, head)
        top.parent = shell
        rim_ = tube(name + "_hoop", [(math.cos(a) * radius * 1.02, math.sin(a) * radius * 1.02, depth / 2)
                                      for a in [i * math.pi / 16 for i in range(33)]], [0.012] * 33, chrome, order=3, caps=False)
        rim_.parent = shell
        parts += [top, rim_]
    return parts
kit += drum("RackTom", 0.17, 0.2, (2.42, 3.6, 0.78), 24)
kit += drum("Snare", 0.18, 0.13, (1.8, 3.3, 0.62), 10)
kit.append(tube("SnareStand", [(1.8, 3.3, 0.0), (1.8, 3.3, 0.55)], [0.012, 0.012], chrome, order=2))
# floor tom under a dust sheet
sheet = cylinder("DustSheet", 0.34, 0.66, (3.45, 3.55, 0.33), 48, mat_dust_sheet(), radius2=0.19)
sub = sheet.modifiers.new("Sub", "SUBSURF")
sub.levels = sub.render_levels = 2
tex = bpy.data.textures.new("SheetWrinkles", "CLOUDS")
tex.noise_scale = 0.06
disp = sheet.modifiers.new("Wrinkles", "DISPLACE")
disp.texture, disp.strength = tex, 0.06
folds = bpy.data.textures.new("SheetFolds", "STUCCI")
folds.noise_scale = 0.12
disp2 = sheet.modifiers.new("Folds", "DISPLACE")
disp2.texture, disp2.strength = folds, 0.04
smooth(sheet, 180)
kit.append(sheet)
# cymbal stands
def cymbal_stand(name, base, height, tilt_deg, radius):
    parts = []
    parts.append(cylinder(f"{name}_pole", 0.012, height, (base[0], base[1], height / 2.0), 16, chrome))
    for k in range(3):
        a = k * 2.0 * math.pi / 3.0 + 0.4
        foot = (base[0] + math.cos(a) * 0.32, base[1] + math.sin(a) * 0.32, 0.0)
        parts.append(tube(f"{name}_leg{k}", [foot, (base[0], base[1], 0.35)], [0.01, 0.01], chrome, order=2))
    c = cylinder(f"{name}_cym", radius, 0.025, (0, 0, 0), 64, bronze, radius2=0.03)
    c.location = (base[0], base[1], height + 0.02)
    c.rotation_euler = (math.radians(tilt_deg), math.radians(tilt_deg * 0.5), 0)
    parts.append(c)
    return parts
kit += cymbal_stand("Crash", (2.0, 4.35), 1.25, 18, 0.23)
kit += cymbal_stand("Ride", (3.55, 4.45), 1.05, 11, 0.27)
kit += cymbal_stand("HiHat", (1.35, 3.7), 0.9, 3, 0.18)
room += kit

# ---------------------------------------------------------------- dust in the moonlight
moon_dir = Vector((1.0, 0.25, -0.72)).normalized()
dust_mat = mat_simple("DustMote", (0.6, 0.6, 0.6), 0.9)
for i in range(260):
    start = Vector((X0, random.uniform(WIN_Y0, WIN_Y1), random.uniform(WIN_Z0, WIN_Z1)))
    jitter = Vector((random.uniform(-0.15, 0.15), random.uniform(-0.15, 0.15), random.uniform(-0.15, 0.15)))
    p = start + moon_dir * random.uniform(0.2, 3.5) + jitter
    if p.z < 0.05:
        continue
    room.append(ellipsoid(f"Mote{i}", (0.004, 0.004, 0.004), tuple(p), dust_mat, 8, 4))

# ---------------------------------------------------------------- the master's body (up to the shoulders)
# The head is always out of frame: the shoulders sit at the top edge, the neck runs up out of view.
TY = 0.6                                         # where he stands, just behind the control bar
SH_Z = solve_height(cam, 0.0, TY - 0.12, 0.985)  # shoulder line just inside the top edge
HIP_Z = SH_Z - 1.2                               # long, very thin torso
trousers, leather = mat_trousers(), mat_leather()
jacket = mat_trousers()

def lofted(name, rings, segs=32, mat=None):
    """Closed body lofted through elliptical rings: (z, rx, ry, y_offset)."""
    bm = bmesh.new()
    loops = []
    for z, rx, ry, yo in rings:
        loops.append([bm.verts.new((rx * math.cos(2 * math.pi * i / segs), TY + yo + ry * math.sin(2 * math.pi * i / segs), z))
                      for i in range(segs)])
    for r0, r1 in zip(loops, loops[1:]):
        for i in range(segs):
            j = (i + 1) % segs
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    bm.faces.new(list(reversed(loops[0])))
    bm.faces.new(loops[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = mesh_obj(name, bm, mat)
    sub = ob.modifiers.new("Sub", "SUBSURF")
    sub.levels = sub.render_levels = 2
    smooth(ob, 180)
    return ob

L_T = SH_Z - HIP_Z
torso = lofted("Torso", [
    (HIP_Z - 0.05,       0.15, 0.10,  0.00),     # hips
    (HIP_Z + 0.22 * L_T, 0.13, 0.09,  0.00),
    (HIP_Z + 0.42 * L_T, 0.10, 0.075, -0.03),    # wasp waist
    (HIP_Z + 0.62 * L_T, 0.15, 0.095, -0.07),    # ribs
    (HIP_Z + 0.82 * L_T, 0.23, 0.105, -0.11),    # upper chest
    (HIP_Z + 0.94 * L_T, 0.29, 0.095, -0.13),    # shoulders
    (SH_Z + 0.02,        0.2,  0.085, -0.13),    # top of the shoulders
    (SH_Z + 0.07,        0.065, 0.06, -0.12),    # base of the neck
    (SH_Z + 0.45,        0.055, 0.05, -0.09),    # neck, out of frame
], mat=jacket)
body = [torso]
body.append(ellipsoid("Pelvis", (0.16, 0.105, 0.13), (0.0, TY, HIP_Z), trousers, 32, 16))
for s_ in (-1, 1):                               # long coat tails hanging behind the legs
    tail = box(f"CoatTail{s_}", (0.12, 0.012, 1.15), (0, 0, 0), jacket)
    tail.location = (s_ * 0.07, TY + 0.12, HIP_Z - 0.5)
    tail.rotation_euler = (math.radians(-6), math.radians(s_ * 4), 0)
    add_bevel(tail, 0.005, 2)
    body.append(tail)

legs = []
for s_ in (-1, 1):
    pts = [(s_ * 0.08, TY, HIP_Z + 0.1), (s_ * 0.18, TY + 0.01, HIP_Z - 0.35), (s_ * 0.34, TY + 0.02, 0.72 * HIP_Z),
           (s_ * 0.42, TY + 0.03, 0.52 * HIP_Z), (s_ * 0.38, TY + 0.04, 0.36 * HIP_Z), (s_ * 0.3, TY + 0.05, 0.17 * HIP_Z),
           (s_ * 0.24, TY + 0.06, 0.17)]
    legs.append(tube(f"Leg{s_}", pts, [0.085, 0.075, 0.065, 0.11, 0.055, 0.045, 0.042], trousers, order=3))
    shoe = [(s_ * 0.24, TY + 0.14, 0.08), (s_ * 0.25, TY - 0.15, 0.075), (s_ * 0.27, TY - 0.5, 0.065),
            (s_ * 0.29, TY - 0.75, 0.1), (s_ * 0.3, TY - 0.82, 0.22), (s_ * 0.29, TY - 0.73, 0.3), (s_ * 0.28, TY - 0.66, 0.28)]
    legs.append(tube(f"Shoe{s_}", shoe, [0.08, 0.072, 0.045, 0.026, 0.016, 0.009, 0.003], leather, order=3))
legs += body
room += legs

# ---------------------------------------------------------------- control bar + hands (foreground)
BAR_Y = -1.5
BAR_Z = solve_height(cam, 0.0, BAR_Y, 0.76)
K = 1.7                                    # hand scale: inhumanly large and long
skin, sleeve_mat = mat_skin(), mat_trousers()
bar_wood = mat_old_wood("ControlBar", (0.07, 0.042, 0.024))
hands = [tube("Bar", [(-0.8, BAR_Y, BAR_Z), (0.8, BAR_Y, BAR_Z)], [0.03, 0.03], bar_wood, order=2),
         tube("BarCross", [(0.0, BAR_Y - 0.3, BAR_Z + 0.032), (0.0, BAR_Y + 0.3, BAR_Z + 0.032)], [0.024, 0.024], bar_wood, order=2)]

nail_mat = mat_simple("Nail", (0.03, 0.028, 0.025), 0.25, coat=0.6)

def P(wx, dx, dy, dz):
    return (wx + dx * K, BAR_Y + dy * K, BAR_Z + dz * K)

def hand(s, wx):
    parts = []
    wrist = Vector(P(wx, 0.0, 0.2, 0.135))
    shoulder = Vector((s * 0.28, TY - 0.14, SH_Z - 0.07))
    cuff = wrist + Vector((0, 0.05, 0.03))
    elbow = shoulder.lerp(cuff, 0.5) + Vector((s * 0.32, 0.1, -0.18))      # insect-like, out to the side
    parts.append(tube(f"Arm{s}", [tuple(shoulder), tuple(shoulder.lerp(elbow, 0.5)), tuple(elbow),
                                  tuple(elbow.lerp(cuff, 0.5)), tuple(cuff)],
                      [0.07, 0.055, 0.068, 0.05, 0.058], sleeve_mat, order=3))
    parts.append(bony(f"Wrist{s}", [tuple(wrist + Vector((0, 0.06, 0.035))), tuple(wrist)], [0.026 * K], [0.024 * K, 0.03 * K], skin))
    knuckles = []
    for k, off in enumerate((-0.062, -0.021, 0.021, 0.062)):
        kn = Vector(P(wx, s * off, 0.075, 0.075))
        knuckles.append(kn)
        # metacarpal: a visible bony ridge from the wrist to the knuckle
        parts.append(bony(f"Meta{s}{k}", [tuple(wrist), tuple(wrist.lerp(kn, 0.5) + Vector((0, 0, 0.006 * K))), tuple(kn)],
                          [0.0075 * K, 0.0068 * K], [0.012 * K, 0.0085 * K, 0.0115 * K], skin))
        L = (1.0, 1.3, 1.18, 0.82)[k]
        splay = (-1.6, -0.5, 0.6, 1.8)[k] * s
        curl = random.uniform(0.2, 1.0)
        joints = [tuple(kn),
                  P(wx, s * off + 0.002 * splay, -0.04, 0.042),                 # over the front of the bar
                  P(wx, s * off + 0.008 * splay, -0.062, -0.085 * L),           # middle joint
                  P(wx, s * off + 0.02 * splay, -0.075 - 0.02 * curl, -0.18 * L),  # last joint
                  P(wx, s * off + 0.03 * splay, -0.11 - 0.05 * curl, -0.225 * L)]  # tip, hooked forward
        parts.append(bony(f"Finger{s}{k}", joints, [r * K for r in (0.0072, 0.0062, 0.0052, 0.0042)],
                          [r * K for r in (0.0115, 0.0105, 0.0092, 0.0078, 0.0022)], skin))
        a, b = Vector(joints[-2]), Vector(joints[-1])
        tip = b + (b - a).normalized() * 0.012 * K
        parts.append(tube(f"Nail{s}{k}", [tuple(a.lerp(b, 0.4)), tuple(b), tuple(tip)],
                          [0.0042 * K, 0.0032 * K, 0.0006 * K], nail_mat, order=3))
    # thin skin stretched over the metacarpals (the bones still show through as ridges)
    mid = (knuckles[1] + knuckles[2]) * 0.5
    web_c = wrist.lerp(mid, 0.55) + Vector((0, 0, -0.004 * K))
    web = ellipsoid(f"HandBack{s}", (0.062 * K, 0.06 * K, 0.009 * K), (0, 0, 0), skin, 32, 16)
    web.location = web_c
    web.rotation_euler = (mid - wrist).to_track_quat("Y", "Z").to_euler()
    parts.append(web)
    # thumb, wrapping under the bar from behind
    thumb = [tuple(wrist + Vector((-s * 0.04 * K, -0.01 * K, -0.02 * K))), P(wx, -s * 0.085, 0.07, 0.0),
             P(wx, -s * 0.08, 0.035, -0.07), P(wx, -s * 0.07, 0.0, -0.12), P(wx, -s * 0.06, -0.02, -0.15)]
    parts.append(bony(f"Thumb{s}", thumb, [r * K for r in (0.009, 0.0075, 0.006, 0.0045)],
                      [r * K for r in (0.012, 0.012, 0.0105, 0.0085, 0.0025)], skin))
    return parts

for s in (-1, 1):
    hands += hand(s, s * 0.42)

# ---------------------------------------------------------------- the mask (only its bottom shows)
MASK_Y = -0.4
A, B, C = 0.4, 0.2, 0.58             # half-width, half-depth (flat mask), half-height
def chin(z):
    """Width factor: the lower face narrows to a pointed chin."""
    return 1.0 - 0.26 * (max(-z, 0.0) / C) ** 1.4

mask = ellipsoid("Mask", (A, B, C), (0, 0, 0), mat_porcelain(), 96, 48)
for v in mask.data.vertices:
    v.co.x *= chin(v.co.z)
mask.rotation_euler = (math.radians(14), 0, 0)    # head bowed

def on_face(x, z, lift=0.004):
    x0 = x / chin(z)
    t = 1.0 - (x0 / A) ** 2 - (z / C) ** 2
    return (x, -B * math.sqrt(max(t, 0.0)) - lift, z)

mouth = [on_face(x, -0.32 + 0.07 * (x / 0.2) ** 2 + 0.018 * (x / 0.2) + 0.006 * math.sin(x * 60.0)) for x in [i * 0.02 for i in range(-10, 11)]]
thread = mat_simple("Thread", (0.03, 0.02, 0.015), 0.8)
groove = mat_simple("Groove", (0.003, 0.002, 0.002), 1.0)
mask_parts = [tube("Smile", mouth, [0.004 + 0.004 * (1.0 - abs(i - 10) / 10.0) for i in range(len(mouth))], groove, order=3)]
for k, (x, _, z) in enumerate(mouth[2:-2:2]):
    tilt, ln = random.uniform(-0.012, 0.012), random.uniform(0.026, 0.045)
    a, b = on_face(x - tilt, z + ln, 0.008), on_face(x + tilt, z - ln, 0.008)
    mask_parts.append(tube(f"Stitch{k}", [a, b], [0.0035, 0.0035], thread, order=2))
for p in mask_parts:
    p.parent = mask
mask_mouth_local = Vector(on_face(0.0, -0.32))
lo, hi = 0.0, 12.0
for _ in range(50):              # place the mask so its smile sits just inside the top edge
    mid = (lo + hi) / 2.0
    mask.location = (0.0, MASK_Y, mid)
    bpy.context.view_layer.update()
    if view_y(cam, mask.matrix_world @ mask_mouth_local) < 0.955:
        lo = mid
    else:
        hi = mid
for o in [mask] + mask_parts:                  # the head stays out of frame: no mask in the scene
    bpy.data.objects.remove(o, do_unlink=True)
mask_objs = []

# ================================================================ light
world = bpy.data.worlds.new("Night")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
set_in(bg, ["Color"], (0.004, 0.006, 0.012, 1.0))
set_in(bg, ["Strength"], 1.0)

moon = sun("Moon", tuple(moon_dir), 2.8, (0.5, 0.64, 1.0), 0.4)
stage = spot("StageLamp", (0.0, -3.6, BAR_Z + 1.4), (0.0, BAR_Y, BAR_Z - 0.2), 110.0, (1.0, 0.86, 0.72), 50.0, 0.7, 0.3)
room_fill = area("RoomFill", (0.0, 2.0, H - 0.1), (0.0, 2.0, 0.0), 4.0, 30.0, (0.5, 0.6, 0.85))
rim = spot("MasterRim", (1.9, 0.0, BAR_Z + 1.2), (0.0, BAR_Y, BAR_Z - 0.1), 320.0, (0.55, 0.68, 1.0), 40.0, 0.8, 0.4)
try:                               # light linking (Blender 4.0+): the rim light only touches the master
    rim_coll = bpy.data.collections.new("RimReceivers")
    for o in hands + mask_objs:
        rim_coll.objects.link(o)
    rim.light_linking.receiver_collection = rim_coll
except Exception as e:
    print("[stage] light linking unavailable, rim light off:", e)
    rim.data.energy = 0.0
corner = spot("CornerGlint", (0.3, 1.2, 3.8), (2.5, 3.8, 0.5), 230.0, (0.6, 0.7, 1.0), 36.0, 0.9, 0.3)

leg_rim = spot("LegRim", (-2.6, TY + 3.6, 3.4), (0.0, TY, 1.6), 520.0, (0.55, 0.68, 1.0), 40.0, 0.8, 0.3)
try:
    leg_coll = bpy.data.collections.new("LegRimReceivers")
    for o in legs:
        leg_coll.objects.link(o)
    leg_rim.light_linking.receiver_collection = leg_coll
except Exception as e:
    print("[stage] light linking unavailable, leg rim off:", e)
    leg_rim.data.energy = 0.0

# --- flicker: the warm lamp and the corner glint dim; the moon does not
for lamp_light in (stage, corner):
    lamp_light.data.energy *= args.lamp
print(f"[stage] lamp level {args.lamp}")

haze = box_minmax("Haze", (X0, Y0, 0.0), (X1, Y1, H), mat_volume())
room.append(haze)

c = scene.cycles
c.max_bounces, c.diffuse_bounces, c.glossy_bounces = 6, 3, 3
c.transmission_bounces, c.volume_bounces, c.transparent_max_bounces = 6, 0, 8
try_set(c, "volume_step_rate", [3.0])

# ================================================================ render layers
layer_objs = {"background": room, "mask": mask_objs, "hands": hands}
all_objs = room + mask_objs + hands

def show_only(objs):
    keep = set(o.name for o in objs)
    for o in all_objs:
        o.hide_render = o.name not in keep

for layer in [l.strip() for l in args.layers.split(",") if l.strip()]:
    if layer == "preview":
        show_only(all_objs)
        setup_cycles(args.samples, WIDTH, HEIGHT, transparent=False)
    else:
        show_only(layer_objs[layer] + ([haze] if layer == "background" else []))
        setup_cycles(args.samples, WIDTH, HEIGHT, transparent=(layer != "background"))
    suffix = "" if abs(args.lamp - 1.0) < 1e-6 else f"_lamp{round(args.lamp * 100):03d}"
    render_to(os.path.join(args.out, f"stage_{layer}{suffix}.png"))
