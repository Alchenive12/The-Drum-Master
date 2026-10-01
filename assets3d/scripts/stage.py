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
    set_in(br, ["Mortar Size"], 0.006)
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
    n = bump(nt, math_node(nt, "ADD", gaps, math_node(nt, "MULTIPLY", grain_h, 0.15)), 0.4, 0.02)
    link_in(nt, n, wood, ["Normal"])
    finish(nt, out, dusty(nt, wood, 0.55, 12.0))
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
                             (0.92, (0.12, 0.12, 0.125)), (0.95, (0.012, 0.012, 0.014))], "CONSTANT")
    worn = ramp(nt, noise(nt, co, 6.0, 8.0, 0.6), [(0.4, gray(0.0)), (0.75, gray(0.5))])
    col = mix_color(nt, worn, col, (0.05, 0.048, 0.045))
    b = principled(nt, (0, 0, 0), 0.8)
    link_in(nt, col, b, ["Base Color"])
    set_in(b, ["Sheen Weight", "Sheen"], 0.6)
    link_in(nt, bump(nt, noise(nt, co, 60.0, 4.0, 0.5), 0.15, 0.005), b, ["Normal"])
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
    col = ramp(nt, blot, [(0.3, (0.36, 0.34, 0.31)), (0.6, (0.5, 0.48, 0.44)), (0.85, (0.56, 0.55, 0.52))])
    b = principled(nt, (0, 0, 0), 0.5)
    link_in(nt, col, b, ["Base Color"])
    set_in(b, ["Subsurface Weight", "Subsurface"], 0.15)
    set_in(b, ["Subsurface Radius"], (0.01, 0.006, 0.004))
    link_in(nt, bump(nt, noise(nt, co, 80.0, 6.0, 0.6), 0.25, 0.002), b, ["Normal"])
    dirt = principled(nt, (0.06, 0.05, 0.04), 0.8)
    finish(nt, out, mix_shaders(nt, math_node(nt, "MULTIPLY", ao_dirt(nt, 0.03), 0.9), b, dirt))
    return m

def mat_porcelain():
    m, nt, out = new_material("CrackedPorcelain")
    co = coords(nt)
    col = ramp(nt, noise(nt, co, 5.0, 10.0, 0.6), [(0.3, (0.55, 0.52, 0.46)), (0.6, (0.72, 0.7, 0.64)), (0.85, (0.78, 0.76, 0.71))])
    b = principled(nt, (0, 0, 0), 0.3)
    link_in(nt, col, b, ["Base Color"])
    set_in(b, ["Coat Weight", "Clearcoat"], 0.35)
    set_in(b, ["Subsurface Weight", "Subsurface"], 0.05)
    edges = voronoi_edges(nt, mapping(nt, co, scale=(1.0, 1.0, 1.6)), 4.0)
    crack_line = ramp(nt, edges, [(0.0, gray(1.0)), (0.01, gray(0.0))])
    where = ramp(nt, noise(nt, co, 2.0, 4.0, 0.5), [(0.5, gray(0.0)), (0.62, gray(1.0))])
    crack = math_node(nt, "MULTIPLY", crack_line, where)
    link_in(nt, bump(nt, math_node(nt, "SUBTRACT", 1.0, crack), 0.6, 0.004), b, ["Normal"])
    dark = principled(nt, (0.02, 0.016, 0.012), 0.9)
    s = mix_shaders(nt, crack, b, dark)
    s = mix_shaders(nt, math_node(nt, "MULTIPLY", ao_dirt(nt, 0.05), 0.8), s, dark)
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
    b = principled(nt, (0.42, 0.4, 0.34), 0.6)
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
    set_in(v, ["Density"], 0.05)
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
bd.location = (2.55, 3.9, 0.31)
kit.append(bd)
bdh = cylinder("BassHead", 0.29, 0.01, (0, 0, 0.215), 64, split_head)   # faces the camera
bdh.parent = bd
kit.append(bdh)
for k, z in enumerate((-0.2, 0.2)):
    hoop = tube(f"BassHoop{k}", [(math.cos(a) * 0.305, math.sin(a) * 0.305, z) for a in [i * math.pi / 16 for i in range(33)]],
                [0.018] * 33, chrome, order=3, caps=False)
    hoop.parent = bd
    kit.append(hoop)
# floor tom under a dust sheet
sheet = cylinder("DustSheet", 0.25, 0.62, (3.45, 3.55, 0.31), 48, mat_dust_sheet(), radius2=0.2)
sub = sheet.modifiers.new("Sub", "SUBSURF")
sub.levels = sub.render_levels = 2
tex = bpy.data.textures.new("SheetWrinkles", "CLOUDS")
tex.noise_scale = 0.08
disp = sheet.modifiers.new("Wrinkles", "DISPLACE")
disp.texture, disp.strength = tex, 0.035
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

# ---------------------------------------------------------------- the master's legs and shoes
trousers, leather = mat_trousers(), mat_leather()
legs = []
for s in (-1, 1):
    pts = [(s * 0.28, 2.95, 6.0), (s * 0.5, 2.92, 4.4), (s * 0.78, 2.88, 2.9), (s * 0.9, 2.86, 2.3),
           (s * 0.82, 2.85, 1.7), (s * 0.58, 2.82, 0.75), (s * 0.42, 2.8, 0.17)]
    legs.append(tube(f"Leg{s}", pts, [0.09, 0.075, 0.07, 0.125, 0.06, 0.045, 0.045], trousers, order=3))
    shoe = [(s * 0.42, 2.92, 0.08), (s * 0.43, 2.62, 0.075), (s * 0.45, 2.22, 0.065),
            (s * 0.48, 1.95, 0.1), (s * 0.49, 1.88, 0.22), (s * 0.47, 1.98, 0.3), (s * 0.45, 2.06, 0.28)]
    legs.append(tube(f"Shoe{s}", shoe, [0.08, 0.072, 0.045, 0.026, 0.016, 0.009, 0.003], leather, order=3))
room += legs

# ---------------------------------------------------------------- control bar + hands (foreground)
BAR_Y = -1.5
BAR_Z = solve_height(cam, 0.0, BAR_Y, 0.76)
K = 1.7                                    # hand scale: inhumanly large and long
skin, sleeve_mat = mat_skin(), mat_trousers()
bar_wood = mat_old_wood("ControlBar", (0.07, 0.042, 0.024))
hands = [tube("Bar", [(-0.8, BAR_Y, BAR_Z), (0.8, BAR_Y, BAR_Z)], [0.03, 0.03], bar_wood, order=2),
         tube("BarCross", [(0.0, BAR_Y - 0.3, BAR_Z + 0.032), (0.0, BAR_Y + 0.3, BAR_Z + 0.032)], [0.024, 0.024], bar_wood, order=2)]

def finger(name, base_x, s, k, length, curl):
    """Knobbly finger: rises from the knuckle behind the bar, drapes over it, hangs down in
    front and hooks back towards the camera at the tip."""
    j = lambda dy, dz, dx=0.0: (base_x + dx * s, BAR_Y + dy * K, BAR_Z + dz * K)
    L = length
    pts = [j(0.09, 0.07), j(0.05, 0.075), j(-0.01, 0.06),
           j(-0.045, 0.02), j(-0.055, -0.06 * L, 0.004),
           j(-0.05, -0.13 * L, 0.008), j(-0.07 - 0.03 * curl, -0.2 * L, 0.012),
           j(-0.1 - 0.05 * curl, -0.235 * L, 0.014)]
    radii = [r * K for r in (0.0125, 0.011, 0.0118, 0.0085, 0.0098, 0.0072, 0.0058, 0.0022)]
    return tube(name, pts, radii, skin, order=3)

for s in (-1, 1):
    wx = s * 0.42
    hands.append(tube(f"Sleeve{s}", [(wx * 1.3, BAR_Y + 1.2, BAR_Z + 1.8), (wx * 1.15, BAR_Y + 0.55, BAR_Z + 0.65),
                                      (wx, BAR_Y + 0.2 * K, BAR_Z + 0.13 * K)],
                      [0.075, 0.07, 0.062], sleeve_mat, order=3))
    hands.append(ellipsoid(f"Palm{s}", (0.05 * K, 0.035 * K, 0.06 * K), (wx, BAR_Y + 0.12 * K, BAR_Z + 0.09 * K), skin, 32, 16))
    for k, off in enumerate((-0.05, -0.017, 0.017, 0.05)):
        length = (1.0, 1.25, 1.15, 0.85)[k]
        hands.append(finger(f"Finger{s}{k}", wx + s * off * K, s, k, length, random.uniform(0.3, 1.0)))
    tb = lambda dx, dy, dz: (wx - s * dx * K, BAR_Y + dy * K, BAR_Z + dz * K)
    hands.append(tube(f"Thumb{s}", [tb(0.05, 0.1, 0.06), tb(0.075, 0.065, -0.01), tb(0.075, 0.03, -0.07),
                                     tb(0.065, 0.0, -0.13), tb(0.055, -0.02, -0.15)],
                      [r * K for r in (0.015, 0.012, 0.012, 0.008, 0.003)], skin, order=3))

# ---------------------------------------------------------------- the mask (only its bottom shows)
MASK_Y = -0.4
A, B, C = 0.42, 0.28, 0.56           # half-width, half-depth, half-height
mask = ellipsoid("Mask", (A, B, C), (0, 0, 0), mat_porcelain())
mask.rotation_euler = (math.radians(14), 0, 0)    # head bowed

def on_face(x, z, lift=0.004):
    t = 1.0 - (x / A) ** 2 - (z / C) ** 2
    return (x, -B * math.sqrt(max(t, 0.0)) - lift, z)

mouth = [on_face(x, -0.3 + 0.13 * (x / 0.28) ** 2) for x in [i * 0.028 for i in range(-10, 11)]]
thread = mat_simple("Thread", (0.03, 0.02, 0.015), 0.8)
groove = mat_simple("Groove", (0.003, 0.002, 0.002), 1.0)
mask_parts = [tube("Smile", mouth, [0.01] * len(mouth), groove, order=3)]
for k, (x, _, z) in enumerate(mouth[1:-1:2]):
    a, b = on_face(x - 0.007, z + 0.05, 0.01), on_face(x + 0.007, z - 0.05, 0.01)
    mask_parts.append(tube(f"Stitch{k}", [a, b], [0.005, 0.005], thread, order=2))
for p in mask_parts:
    p.parent = mask
mask_mouth_local = Vector(on_face(0.0, -0.3))
lo, hi = 0.0, 12.0
for _ in range(50):              # place the mask so its smile sits just inside the top edge
    mid = (lo + hi) / 2.0
    mask.location = (0.0, MASK_Y, mid)
    bpy.context.view_layer.update()
    if view_y(cam, mask.matrix_world @ mask_mouth_local) < 0.9:
        lo = mid
    else:
        hi = mid
mask_objs = [mask] + mask_parts

# ================================================================ light
world = bpy.data.worlds.new("Night")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
set_in(bg, ["Color"], (0.004, 0.006, 0.012, 1.0))
set_in(bg, ["Strength"], 1.0)

moon = sun("Moon", tuple(moon_dir), 3.0, (0.5, 0.64, 1.0), 0.4)
stage = spot("StageLamp", (0.0, -3.6, BAR_Z + 1.4), (0.0, BAR_Y, BAR_Z - 0.2), 160.0, (1.0, 0.72, 0.45), 50.0, 0.7, 0.3)
room_fill = area("RoomFill", (0.0, 2.0, H - 0.1), (0.0, 2.0, 0.0), 4.0, 140.0, (0.5, 0.6, 0.85))
corner = spot("CornerGlint", (0.5, 1.5, 3.8), (2.8, 4.0, 0.5), 260.0, (0.6, 0.7, 1.0), 32.0, 0.9, 0.3)

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
    render_to(os.path.join(args.out, f"stage_{layer}.png"))
