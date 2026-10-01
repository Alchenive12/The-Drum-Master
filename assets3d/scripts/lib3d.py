# File: assets3d/scripts/lib3d.py
# Shared helpers for the asset scripts: version-safe node/material helpers (Blender 4.0 .. 4.5+),
# simple mesh and curve builders, lights, camera framing and render setup.

import math
import os

import bmesh
import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

# ---------------------------------------------------------------- sockets / nodes
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

def coords(nt, kind="Object"):
    tc = nt.nodes.new("ShaderNodeTexCoord")
    return tc.outputs[kind]

def mapping(nt, vec, scale=(1, 1, 1), rot=(0, 0, 0), loc=(0, 0, 0)):
    m = nt.nodes.new("ShaderNodeMapping")
    set_in(m, ["Scale"], scale)
    set_in(m, ["Rotation"], rot)
    set_in(m, ["Location"], loc)
    nt.links.new(vec, m.inputs["Vector"])
    return m.outputs["Vector"]

def noise(nt, vec, scale, detail=8.0, rough=0.6):
    n = nt.nodes.new("ShaderNodeTexNoise")
    set_in(n, ["Scale"], scale)
    set_in(n, ["Detail"], detail)
    set_in(n, ["Roughness"], rough)
    nt.links.new(vec, n.inputs["Vector"])
    return n.outputs["Fac"]

def voronoi_edges(nt, vec, scale):
    v = nt.nodes.new("ShaderNodeTexVoronoi")
    v.feature = "DISTANCE_TO_EDGE"
    set_in(v, ["Scale"], scale)
    nt.links.new(vec, v.inputs["Vector"])
    return v.outputs["Distance"]

def wave_bands(nt, vec, scale, axis="X"):
    w = nt.nodes.new("ShaderNodeTexWave")
    w.wave_type = "BANDS"
    w.bands_direction = axis
    try_set(w, "wave_profile", ["SAW"])
    set_in(w, ["Scale"], scale)
    set_in(w, ["Distortion"], 0.0)
    nt.links.new(vec, w.inputs["Vector"])
    return w.outputs["Fac"]

def ramp(nt, fac_socket, stops, interp="LINEAR"):
    """stops: list of (position, (r, g, b)). Returns the Color output."""
    r = nt.nodes.new("ShaderNodeValToRGB")
    r.color_ramp.interpolation = interp
    els = r.color_ramp.elements
    els[0].position, els[0].color = stops[0][0], (*stops[0][1], 1.0)
    els[1].position, els[1].color = stops[-1][0], (*stops[-1][1], 1.0)
    for pos, col in stops[1:-1]:
        e = els.new(pos)
        e.color = (*col, 1.0)
    nt.links.new(fac_socket, r.inputs["Fac"])
    return r.outputs["Color"]

def gray(v):
    return (v, v, v)

def math_node(nt, op, a, b=None, clamp=False):
    m = nt.nodes.new("ShaderNodeMath")
    m.operation = op
    m.use_clamp = clamp
    for i, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            m.inputs[i].default_value = float(v)
        else:
            nt.links.new(v, m.inputs[i])
    return m.outputs[0]

def mix_color(nt, fac, a, b, blend="MIX"):
    """Colour mix that works across Blender versions (ShaderNodeMix sockets are found by type)."""
    m = nt.nodes.new("ShaderNodeMix")
    m.data_type = "RGBA"
    m.blend_type = blend
    fac_s = [s for s in m.inputs if s.enabled and s.type == "VALUE"][0]
    col_s = [s for s in m.inputs if s.enabled and s.type == "RGBA"]
    for target, v in ((fac_s, fac), (col_s[0], a), (col_s[1], b)):
        if isinstance(v, (int, float)):
            target.default_value = float(v)
        elif isinstance(v, tuple):
            target.default_value = (*v, 1.0) if len(v) == 3 else v
        else:
            nt.links.new(v, target)
    return [s for s in m.outputs if s.enabled and s.type == "RGBA"][0]

def bump(nt, height, strength, distance=0.02, normal=None):
    b = nt.nodes.new("ShaderNodeBump")
    set_in(b, ["Strength"], strength)
    set_in(b, ["Distance"], distance)
    nt.links.new(height, b.inputs["Height"])
    if normal is not None:
        nt.links.new(normal, b.inputs["Normal"])
    return b.outputs["Normal"]

def mix_shaders(nt, fac, a, b):
    m = nt.nodes.new("ShaderNodeMixShader")
    if isinstance(fac, (int, float)):
        m.inputs[0].default_value = float(fac)
    else:
        nt.links.new(fac, m.inputs[0])
    nt.links.new(a.outputs[0] if hasattr(a, "outputs") else a, m.inputs[1])
    nt.links.new(b.outputs[0] if hasattr(b, "outputs") else b, m.inputs[2])
    return m

def ao_dirt(nt, distance, power=1.5):
    """~1 in crevices, ~0 on open surfaces."""
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    set_in(ao, ["Distance"], distance)
    inv = math_node(nt, "SUBTRACT", 1.0, ao.outputs["AO"])
    return math_node(nt, "POWER", inv, power)

def up_facing(nt, sharpness=4.0):
    """~1 on surfaces facing up (where dust settles), 0 on walls and undersides."""
    g = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(g.outputs["Normal"], sep.inputs[0])
    z = math_node(nt, "MAXIMUM", sep.outputs["Z"], 0.0)
    return math_node(nt, "POWER", z, sharpness)

def dusty(nt, shader, amount=0.6, scale=30.0):
    """Lays grey dust over the upward-facing parts of `shader`. Returns the mix-shader node."""
    co = coords(nt)
    speck = noise(nt, co, scale, 6.0, 0.7)
    mask = math_node(nt, "MULTIPLY", up_facing(nt), math_node(nt, "MULTIPLY", speck, amount * 1.6), clamp=True)
    dust = principled(nt, (0.16, 0.15, 0.135), 0.95)
    return mix_shaders(nt, mask, shader, dust)

def finish(nt, out, shader):
    nt.links.new(shader.outputs[0] if hasattr(shader, "outputs") else shader, out.inputs["Surface"])

# ---------------------------------------------------------------- geometry
def link(ob):
    bpy.context.scene.collection.objects.link(ob)
    return ob

def mesh_obj(name, bm, mat=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new(name, me))
    if mat is not None:
        ob.data.materials.append(mat)
    return ob

def box(name, size, center, mat=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=size, verts=bm.verts)
    bmesh.ops.translate(bm, vec=center, verts=bm.verts)
    return mesh_obj(name, bm, mat)

def box_minmax(name, lo, hi, mat=None):
    size = tuple(h - l for l, h in zip(lo, hi))
    center = tuple((l + h) / 2.0 for l, h in zip(lo, hi))
    return box(name, size, center, mat)

def cylinder(name, radius, depth, center=(0, 0, 0), segments=64, mat=None, radius2=None):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments,
                          radius1=radius, radius2=radius if radius2 is None else radius2, depth=depth)
    bmesh.ops.translate(bm, vec=center, verts=bm.verts)
    return mesh_obj(name, bm, mat)

def ellipsoid(name, axes, center=(0, 0, 0), mat=None, u=64, v=32):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=1.0)
    bmesh.ops.scale(bm, vec=axes, verts=bm.verts)
    bmesh.ops.translate(bm, vec=center, verts=bm.verts)
    ob = mesh_obj(name, bm, mat)
    smooth(ob, 180)
    return ob

def tube(name, pts, radii, mat=None, order=4, res_u=16, bevel_res=6, caps=True):
    """Smooth tube through control points with a radius per point (NURBS curve, bevelled)."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = 1.0
    cu.bevel_resolution = bevel_res
    cu.resolution_u = res_u
    cu.use_fill_caps = caps
    sp = cu.splines.new("NURBS")
    sp.points.add(len(pts) - 1)
    for p, co, r in zip(sp.points, pts, radii):
        p.co = (co[0], co[1], co[2], 1.0)
        p.radius = r
    sp.order_u = min(order, len(pts))
    sp.use_endpoint_u = True
    sp.use_smooth = True
    ob = link(bpy.data.objects.new(name, cu))
    if mat is not None:
        ob.data.materials.append(mat)
    return ob

def smooth(obj, angle_deg=35.0):
    if obj.type != "MESH":
        return
    for p in obj.data.polygons:
        p.use_smooth = True
    if hasattr(obj.data, "use_auto_smooth"):            # Blender <= 4.0
        obj.data.use_auto_smooth = True
        obj.data.auto_smooth_angle = math.radians(angle_deg)
    elif angle_deg < 179.0:                             # Blender 4.1+
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

# ---------------------------------------------------------------- lights
def spot(name, loc, target, energy, color, size_deg, blend=0.5, radius=0.1):
    ld = bpy.data.lights.new(name, "SPOT")
    ld.energy, ld.color = energy, color
    ld.spot_size, ld.spot_blend = math.radians(size_deg), blend
    ld.shadow_soft_size = radius
    ob = link(bpy.data.objects.new(name, ld))
    ob.location = loc
    look_at(ob, target)
    return ob

def area(name, loc, target, size, energy, color):
    ld = bpy.data.lights.new(name, "AREA")
    ld.shape, ld.size, ld.energy, ld.color = "SQUARE", size, energy, color
    ob = link(bpy.data.objects.new(name, ld))
    ob.location = loc
    look_at(ob, target)
    return ob

def sun(name, direction, strength, color, angle_deg=0.5):
    ld = bpy.data.lights.new(name, "SUN")
    ld.energy, ld.color = strength, color
    ld.angle = math.radians(angle_deg)
    ob = link(bpy.data.objects.new(name, ld))
    ob.rotation_euler = Vector(direction).normalized().to_track_quat("-Z", "Y").to_euler()
    return ob

# ---------------------------------------------------------------- camera framing
def view_y(cam, point):
    """Vertical position of a world point in the camera frame: 0 = bottom edge, 1 = top edge."""
    return world_to_camera_view(bpy.context.scene, cam, Vector(point)).y

def solve_height(cam, x, y, target_view_y, lo=-5.0, hi=15.0):
    """World z such that (x, y, z) lands at target_view_y in the frame."""
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if view_y(cam, (x, y, mid)) < target_view_y:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2.0

# ---------------------------------------------------------------- render
def setup_cycles(samples, res_x, res_y, transparent=False):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_adaptive_sampling = True

    # Probe the denoiser with a tiny render: distro builds can lack OpenImageDenoise.
    scene.render.resolution_x = scene.render.resolution_y = 16
    scene.cycles.samples = 1
    scene.cycles.use_denoising = True
    try_set(scene.cycles, "denoiser", ["OPENIMAGEDENOISE"])
    try:
        bpy.ops.render.render(write_still=False)
        has_oidn = True
    except RuntimeError:
        has_oidn = False
    scene.cycles.use_denoising = has_oidn
    scene.cycles.samples = samples if has_oidn else samples * 4

    scene.render.resolution_x, scene.render.resolution_y = res_x, res_y
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = transparent
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA" if transparent else "RGB"
    vt = try_set(scene.view_settings, "view_transform", ["AgX", "Filmic"])
    print(f"[lib3d] Blender {bpy.app.version_string}, {vt}, denoise {has_oidn}, "
          f"{res_x}x{res_y}, {scene.cycles.samples} samples")
    return has_oidn

def render_to(path):
    bpy.ops.render.render(write_still=False)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.data.images["Render Result"].save_render(path, scene=bpy.context.scene)
    print("[lib3d] wrote", path)
