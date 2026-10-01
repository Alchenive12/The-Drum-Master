# File: assets3d/scripts/knob_test.py
#
# Test render of ONE knob for The Drum Master, to judge the 3D look before building every asset.
# Same design as the SVG knob (assets/ui/svg/knob_body.svg): black lacquered cap, tarnished knurled
# brass skirt, bone pointer with a red jewel. Low-key horror lighting: dim warm stage key from the
# top-left, cold moonlight grazing from the right.
#
# Runs headless in CI:  blender -b --factory-startup -P assets3d/scripts/knob_test.py -- --out <dir>
# Written to work on Blender 4.0 (Ubuntu 24.04 apt) and newer: every API that changed between
# versions goes through a fallback helper.
#
# Units: 1 Blender unit = 1 cm. The knob is 3.4 cm across.
#
# Output (in --out):
#   knob_v000.png, knob_v050.png, knob_v100.png    knob at min / mid / max

import argparse
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

# ---------------------------------------------------------------- args
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--out", default="assets3d/renders/knob_test")
ap.add_argument("--samples", type=int, default=128)
ap.add_argument("--res", type=int, default=640)
args = ap.parse_args(argv)
os.makedirs(args.out, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

# ---------------------------------------------------------------- version-safe helpers
def sock(node, names):
    for n in names:
        s = node.inputs.get(n)
        if s is not None:
            return s
    return None

def set_in(node, names, value):
    s = sock(node, names)
    if s is not None:
        s.default_value = value
    return s is not None

def link_in(nt, out_socket, node, names):
    s = sock(node, names)
    if s is not None:
        nt.links.new(out_socket, s)
    return s is not None

def try_set(obj, attr, values):
    for v in values:
        try:
            setattr(obj, attr, v)
            return v
        except (TypeError, ValueError, AttributeError):
            continue
    return None

def smooth(obj, angle_deg=35.0):
    for p in obj.data.polygons:
        p.use_smooth = True
    if hasattr(obj.data, "use_auto_smooth"):            # Blender <= 4.0
        obj.data.use_auto_smooth = True
        obj.data.auto_smooth_angle = math.radians(angle_deg)
    else:                                               # Blender 4.1+
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        try:
            bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle_deg))
        except Exception:
            pass

def add_bevel(obj, width, segments=3, angle_deg=30.0):
    m = obj.modifiers.new("Bevel", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.angle_limit = math.radians(angle_deg)
    return m

def look_at(obj, target):
    d = Vector(target) - obj.location
    obj.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()

def new_material(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    return m, nt, out

def principled(nt, color, rough, metal=0.0):
    b = nt.nodes.new("ShaderNodeBsdfPrincipled")
    set_in(b, ["Base Color"], (*color, 1.0))
    set_in(b, ["Roughness"], rough)
    set_in(b, ["Metallic"], metal)
    return b

def obj_coords(nt):
    tc = nt.nodes.new("ShaderNodeTexCoord")
    return tc.outputs["Object"]

def noise(nt, vec, scale, detail=8.0, rough=0.6):
    n = nt.nodes.new("ShaderNodeTexNoise")
    set_in(n, ["Scale"], scale)
    set_in(n, ["Detail"], detail)
    set_in(n, ["Roughness"], rough)
    nt.links.new(vec, n.inputs["Vector"])
    return n

def ramp(nt, fac_socket, stops):
    """stops: list of (position, (r, g, b))"""
    r = nt.nodes.new("ShaderNodeValToRGB")
    els = r.color_ramp.elements
    els[0].position, els[0].color = stops[0][0], (*stops[0][1], 1.0)
    els[1].position, els[1].color = stops[-1][0], (*stops[-1][1], 1.0)
    for pos, col in stops[1:-1]:
        e = els.new(pos)
        e.color = (*col, 1.0)
    nt.links.new(fac_socket, r.inputs["Fac"])
    return r

def math_node(nt, op, a, b=None):
    m = nt.nodes.new("ShaderNodeMath")
    m.operation = op
    if isinstance(a, float):
        m.inputs[0].default_value = a
    else:
        nt.links.new(a, m.inputs[0])
    if b is not None:
        if isinstance(b, float):
            m.inputs[1].default_value = b
        else:
            nt.links.new(b, m.inputs[1])
    return m.outputs[0]

def bump(nt, height_socket, strength, distance=0.02):
    b = nt.nodes.new("ShaderNodeBump")
    set_in(b, ["Strength"], strength)
    set_in(b, ["Distance"], distance)
    nt.links.new(height_socket, b.inputs["Height"])
    return b.outputs["Normal"]

def mix_shaders(nt, fac_socket, a, b):
    m = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(fac_socket, m.inputs[0])
    nt.links.new(a.outputs[0], m.inputs[1])
    nt.links.new(b.outputs[0], m.inputs[2])
    return m

def ao_dirt(nt, distance):
    """1 in crevices, 0 on open surfaces."""
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    set_in(ao, ["Distance"], distance)
    inv = math_node(nt, "SUBTRACT", 1.0, ao.outputs["AO"])
    return math_node(nt, "POWER", inv, 1.5)

# ---------------------------------------------------------------- materials
def mat_panel():
    m, nt, out = new_material("Panel_PaintedRust")
    co = obj_coords(nt)
    big = noise(nt, co, 0.35, 10.0, 0.62)          # where paint is chipped
    fine = noise(nt, co, 4.0, 14.0, 0.7)           # rust grain
    grime = noise(nt, co, 1.2, 6.0, 0.5)           # streaky grime
    chip_mask = ramp(nt, big.outputs["Fac"], [(0.55, (0, 0, 0)), (0.66, (1, 1, 1))])
    paint = principled(nt, (0.012, 0.014, 0.019), 0.55)
    rust = principled(nt, (0.06, 0.028, 0.012), 0.9)
    rust_col = ramp(nt, fine.outputs["Fac"], [(0.3, (0.018, 0.009, 0.005)), (0.5, (0.055, 0.026, 0.011)), (0.72, (0.1, 0.05, 0.022))])
    link_in(nt, rust_col.outputs["Color"], rust, ["Base Color"])
    # damp vertical streaks (stretched noise) darken the paint and change its sheen
    smp = nt.nodes.new("ShaderNodeMapping")
    set_in(smp, ["Scale"], (5.0, 0.25, 1.0))
    nt.links.new(co, smp.inputs["Vector"])
    streak = ramp(nt, noise(nt, smp.outputs["Vector"], 1.6, 6.0, 0.55).outputs["Fac"], [(0.4, (0, 0, 0)), (0.65, (1, 1, 1))])
    paint_col = ramp(nt, streak.outputs["Color"], [(0.0, (0.014, 0.016, 0.021)), (1.0, (0.006, 0.007, 0.009))])
    link_in(nt, paint_col.outputs["Color"], paint, ["Base Color"])
    rough = ramp(nt, grime.outputs["Fac"], [(0.3, (0.32, 0.32, 0.32)), (0.7, (0.7, 0.7, 0.7))])
    link_in(nt, rough.outputs["Color"], paint, ["Roughness"])
    height = math_node(nt, "ADD", math_node(nt, "MULTIPLY", chip_mask.outputs["Color"], 0.6), math_node(nt, "MULTIPLY", fine.outputs["Fac"], 0.4))
    n = bump(nt, height, 0.35, 0.05)
    link_in(nt, n, paint, ["Normal"])
    link_in(nt, n, rust, ["Normal"])
    mix = mix_shaders(nt, chip_mask.outputs["Color"], paint, rust)
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m

def mat_brass():
    m, nt, out = new_material("Brass_Tarnished")
    co = obj_coords(nt)
    n1 = noise(nt, co, 6.0, 10.0, 0.6)
    brass = principled(nt, (0.42, 0.29, 0.11), 0.32, metal=1.0)
    brass_col = ramp(nt, n1.outputs["Fac"], [(0.3, (0.22, 0.14, 0.05)), (0.55, (0.42, 0.29, 0.11)), (0.75, (0.5, 0.37, 0.16))])
    link_in(nt, brass_col.outputs["Color"], brass, ["Base Color"])
    rough = ramp(nt, n1.outputs["Fac"], [(0.3, (0.22, 0.22, 0.22)), (0.8, (0.5, 0.5, 0.5))])
    link_in(nt, rough.outputs["Color"], brass, ["Roughness"])
    verd = principled(nt, (0.09, 0.2, 0.16), 0.85)
    dirt = principled(nt, (0.025, 0.02, 0.014), 0.9)
    crevice = ao_dirt(nt, 0.15)
    patches = ramp(nt, noise(nt, co, 2.5, 8.0, 0.65).outputs["Fac"], [(0.55, (0, 0, 0)), (0.62, (1, 1, 1))])
    verd_mask = math_node(nt, "MINIMUM", math_node(nt, "ADD", math_node(nt, "MULTIPLY", crevice, 0.8), math_node(nt, "MULTIPLY", patches.outputs["Color"], 0.7)), 1.0)
    a = mix_shaders(nt, verd_mask, brass, verd)
    dirty = mix_shaders(nt, math_node(nt, "MULTIPLY", crevice, 0.6), a, dirt)
    nt.links.new(dirty.outputs[0], out.inputs["Surface"])
    return m

def mat_lacquer():
    m, nt, out = new_material("Black_Lacquer")
    co = obj_coords(nt)
    mp = nt.nodes.new("ShaderNodeMapping")
    set_in(mp, ["Scale"], (0.25, 18.0, 1.0))          # stretched noise -> fine scratches
    set_in(mp, ["Rotation"], (0.0, 0.0, math.radians(23)))
    nt.links.new(co, mp.inputs["Vector"])
    scr = noise(nt, mp.outputs["Vector"], 9.0, 4.0, 0.5)
    scr_r = ramp(nt, scr.outputs["Fac"], [(0.66, (0.03, 0.03, 0.03)), (0.69, (0.3, 0.3, 0.3))])
    smudge = ramp(nt, noise(nt, co, 1.5, 4.0, 0.5).outputs["Fac"], [(0.35, (0.12, 0.12, 0.12)), (0.7, (0.3, 0.3, 0.3))])
    b = principled(nt, (0.006, 0.006, 0.007), 0.2)
    link_in(nt, smudge.outputs["Color"], b, ["Roughness"])
    set_in(b, ["Coat Weight", "Clearcoat"], 1.0)
    link_in(nt, scr_r.outputs["Color"], b, ["Coat Roughness", "Clearcoat Roughness"])
    nt.links.new(b.outputs[0], out.inputs["Surface"])
    return m

def mat_bone():
    m, nt, out = new_material("Bone_Ivory")
    co = obj_coords(nt)
    n1 = noise(nt, co, 9.0, 10.0, 0.6)
    b = principled(nt, (0.74, 0.68, 0.53), 0.45)
    col = ramp(nt, n1.outputs["Fac"], [(0.3, (0.55, 0.48, 0.34)), (0.55, (0.74, 0.68, 0.53)), (0.8, (0.82, 0.77, 0.64))])
    link_in(nt, col.outputs["Color"], b, ["Base Color"])
    set_in(b, ["Subsurface Weight", "Subsurface"], 0.12)
    dirt = principled(nt, (0.08, 0.06, 0.035), 0.8)
    mix = mix_shaders(nt, math_node(nt, "MULTIPLY", ao_dirt(nt, 0.1), 0.85), b, dirt)
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m

def mat_jewel():
    m, nt, out = new_material("Red_Jewel")
    b = principled(nt, (0.4, 0.0, 0.012), 0.02)
    set_in(b, ["Transmission Weight", "Transmission"], 1.0)
    set_in(b, ["IOR"], 1.77)
    set_in(b, ["Emission Color", "Emission"], (0.7, 0.01, 0.02, 1.0))
    set_in(b, ["Emission Strength"], 0.25)
    nt.links.new(b.outputs[0], out.inputs["Surface"])
    return m

# ---------------------------------------------------------------- geometry
def mesh_obj(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    return ob

def knurled_skirt(r_out=1.7, r_in=1.665, ridges=180, h=0.55, taper=0.965):
    bm = bmesh.new()
    n = ridges * 2
    bot, top = [], []
    for i in range(n):
        a = 2.0 * math.pi * i / n
        r = r_out if i % 2 == 0 else r_in
        bot.append(bm.verts.new((r * math.cos(a), r * math.sin(a), 0.0)))
        top.append(bm.verts.new((r * taper * math.cos(a), r * taper * math.sin(a), h)))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bm.faces.new(list(reversed(bot)))
    bm.faces.new(top)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return mesh_obj("Skirt", bm)

def cylinder(name, radius, depth, z0, verts=128):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=verts,
                          radius1=radius, radius2=radius, depth=depth)
    bmesh.ops.translate(bm, vec=(0.0, 0.0, z0 + depth / 2.0), verts=bm.verts)
    return mesh_obj(name, bm)

def box(name, sx, sy, sz, loc):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    bmesh.ops.translate(bm, vec=loc, verts=bm.verts)
    return mesh_obj(name, bm)

def sphere(name, radius, loc, squash=1.0):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=radius)
    bmesh.ops.scale(bm, vec=(1.0, 1.0, squash), verts=bm.verts)
    bmesh.ops.translate(bm, vec=loc, verts=bm.verts)
    return mesh_obj(name, bm)

# Panel
panel = box("Panel", 30.0, 30.0, 0.2, (0.0, 0.0, -0.1))
panel.data.materials.append(mat_panel())

# Knob rig: everything that turns is parented to this empty
rig = bpy.data.objects.new("KnobRig", None)
scene.collection.objects.link(rig)

SKIRT_H = 0.55
CAP_H = 0.9
CAP_TOP = SKIRT_H + 0.1 + CAP_H

skirt = knurled_skirt(h=SKIRT_H)
brass = mat_brass()
skirt.data.materials.append(brass)
add_bevel(skirt, 0.008, 2)
smooth(skirt, 30)

collar = cylinder("Collar", 1.5, 0.14, SKIRT_H - 0.02)
collar.data.materials.append(brass)
add_bevel(collar, 0.06, 5, 40)
smooth(collar, 40)

cap = cylinder("Cap", 1.36, CAP_H, SKIRT_H + 0.1)
cap.data.materials.append(mat_lacquer())
add_bevel(cap, 0.14, 6, 40)
smooth(cap, 40)

bone = mat_bone()
pointer = box("Pointer", 0.17, 1.0, 0.12, (0.0, 0.64, CAP_TOP + 0.05))
pointer.data.materials.append(bone)
add_bevel(pointer, 0.05, 4, 60)
smooth(pointer, 60)

hub = cylinder("Hub", 0.2, 0.13, CAP_TOP - 0.01, verts=64)
hub.data.materials.append(bone)
add_bevel(hub, 0.05, 4, 40)
smooth(hub, 40)

jewel = sphere("Jewel", 0.1, (0.0, 1.0, CAP_TOP + 0.13), squash=0.65)
jewel.data.materials.append(mat_jewel())
smooth(jewel, 180)

for ob in (skirt, collar, cap, pointer, hub, jewel):
    ob.parent = rig

# ---------------------------------------------------------------- lights, world, camera
def area(name, loc, size, energy, color):
    ld = bpy.data.lights.new(name, "AREA")
    ld.shape = "SQUARE"
    ld.size = size
    ld.energy = energy
    ld.color = color
    ob = bpy.data.objects.new(name, ld)
    scene.collection.objects.link(ob)
    ob.location = loc
    look_at(ob, (0.0, 0.0, 0.5))
    return ob

def spot(name, loc, energy, color, size_deg, blend):
    ld = bpy.data.lights.new(name, "SPOT")
    ld.energy = energy
    ld.color = color
    ld.spot_size = math.radians(size_deg)
    ld.spot_blend = blend
    ld.shadow_soft_size = 0.4
    ob = bpy.data.objects.new(name, ld)
    scene.collection.objects.link(ob)
    ob.location = loc
    look_at(ob, (0.0, 0.0, 0.4))
    return ob

spot("KeyLamp", (-6.0, 5.0, 14.0), 3800.0, (1.0, 0.74, 0.48), 16.0, 0.9)    # pool of warm light
spot("MoonRim", (16.0, -2.0, 2.2), 12000.0, (0.45, 0.62, 1.0), 20.0, 0.7)  # cold, grazing
area("Softbox", (-3.0, 3.0, 12.0), 2.5, 260.0, (1.0, 0.85, 0.7))            # highlight in the lacquer
area("Fill", (4.0, -10.0, 9.0), 8.0, 40.0, (0.55, 0.65, 0.9))

world = bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
set_in(bg, ["Color"], (0.003, 0.004, 0.007, 1.0))
set_in(bg, ["Strength"], 1.0)

cam_data = bpy.data.cameras.new("Cam")
cam_data.lens = 60.0
cam_data.sensor_width = 36.0
cam = bpy.data.objects.new("Cam", cam_data)
scene.collection.objects.link(cam)
cam.location = (0.0, 0.0, 9.8)
look_at(cam, (0.0, 0.0, 0.0))
cam.rotation_euler[2] = 0.0
scene.camera = cam

# ---------------------------------------------------------------- render settings
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = args.samples
scene.cycles.use_adaptive_sampling = True
# Distro builds (e.g. Ubuntu's apt Blender) can lack OpenImageDenoise; rendering then fails if
# denoising is on. Without it, fall back to 4x the samples.
def denoiser_works():
    scene.render.resolution_x = scene.render.resolution_y = 16
    old = scene.cycles.samples
    scene.cycles.samples = 1
    scene.cycles.use_denoising = True
    try_set(scene.cycles, "denoiser", ["OPENIMAGEDENOISE"])
    try:
        bpy.ops.render.render(write_still=False)
        ok = True
    except RuntimeError:
        ok = False
    scene.cycles.samples = old
    return ok

HAS_OIDN = denoiser_works()
scene.render.resolution_x = args.res
scene.render.resolution_y = args.res
scene.render.resolution_percentage = 100
scene.cycles.use_denoising = HAS_OIDN
if not HAS_OIDN:
    scene.cycles.samples = args.samples * 4

scene.render.film_transparent = False
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGB"
vt = try_set(scene.view_settings, "view_transform", ["AgX", "Filmic"])
scene.view_settings.exposure = 0.0
print(f"[knob_test] Blender {bpy.app.version_string}, view transform {vt}, "
      f"denoise {scene.cycles.use_denoising}")

# ---------------------------------------------------------------- render
def angle_for(value):
    """value 0..1 -> pointer from 7:30 (min) clockwise through 12:00 to 4:30 (max)."""
    clockwise_deg = -135.0 + 270.0 * value
    return -math.radians(clockwise_deg)     # Blender +Z rotation is counter-clockwise from above

def save(name):
    path = os.path.join(args.out, name)
    bpy.data.images["Render Result"].save_render(path, scene=scene)
    print("[knob_test] wrote", path)

for value, tag in ((0.0, "v000"), (1.0, "v100"), (0.5, "v050")):
    rig.rotation_euler = (0.0, 0.0, angle_for(value))
    scene.view_settings.exposure = 0.0
    bpy.ops.render.render(write_still=False)
    save(f"knob_{tag}.png")

