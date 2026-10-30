"""
AFTERIMAGE - Blender asset generator.

Builds simple, original low-poly props (and the Tenant) from primitives, then renders
each one with an orthographic 3/4 top-down camera on a transparent background.

Run with Blender (3.6 LTS or 4.x):
    blender --background --python blender/render_props.py -- --out assets_src/renders

Then convert the renders into game sprites (16 px wide, game palette, outline):
    python tools/pixelate_sprites.py assets_src/renders assets/sprites

Blender is only an authoring tool. The game never needs Blender to run: it loads the
PNGs from assets/sprites and falls back to procedural pixel art for anything missing.

Render settings (why):
  * Orthographic camera, tilted 30 degrees from straight down -> matches the game's
    3/4 top-down view and keeps every prop the same scale (1 Blender unit = 1 tile).
  * Cycles on CPU, 24 samples, BOX pixel filter of 0.01 px -> crisp silhouettes, works
    headless on any PC without a GPU.
  * Film -> Transparent, PNG RGBA, Standard view transform (no Filmic/AgX tone curve,
    so the colours stay close to the palette).
  * Low-roughness-free, flat materials in palette colours + a violet world light so the
    shadows land in the game's purple range.
"""
import math
import os
import sys

import bpy

# ---------------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------------
def output_dir():
    argv = sys.argv
    if "--" in argv:
        rest = argv[argv.index("--") + 1:]
        if "--out" in rest and rest.index("--out") + 1 < len(rest):
            return os.path.abspath(rest[rest.index("--out") + 1])
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.abspath(os.path.join(here, "..", "assets_src", "renders"))


def only_list():
    argv = sys.argv
    if "--" in argv:
        rest = argv[argv.index("--") + 1:]
        if "--only" in rest and rest.index("--only") + 1 < len(rest):
            return rest[rest.index("--only") + 1].split(",")
    return None


# ---------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------
def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(name, rgb, emission=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    lin = tuple(srgb_to_linear(v) for v in rgb) + (1.0,)
    bsdf.inputs["Base Color"].default_value = lin
    bsdf.inputs["Roughness"].default_value = 1.0
    for key in ("Specular IOR Level", "Specular"):  # 4.x / 3.x names
        if key in bsdf.inputs:
            bsdf.inputs[key].default_value = 0.0
    if emission > 0.0:
        for key in ("Emission Color", "Emission"):
            if key in bsdf.inputs:
                bsdf.inputs[key].default_value = lin
                break
        if "Emission Strength" in bsdf.inputs:
            bsdf.inputs["Emission Strength"].default_value = emission
    m.diffuse_color = lin  # also used by Workbench / viewport
    return m


def box(name, dims, loc, mat):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = dims
    o.data.materials.append(mat)
    return o


def cylinder(name, radius, depth, loc, mat, verts=8):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth, location=loc)
    o = bpy.context.object
    o.name = name
    o.data.materials.append(mat)
    return o


def sphere(name, radius, loc, mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=8, ring_count=6, radius=radius, location=loc)
    o = bpy.context.object
    o.name = name
    o.data.materials.append(mat)
    return o


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def setup(res_x, res_y, ortho_scale, target_z):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 24
    sc.cycles.use_denoising = False
    sc.cycles.pixel_filter_type = "BOX"
    sc.cycles.filter_width = 0.01
    sc.render.resolution_x = res_x
    sc.render.resolution_y = res_y
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "Standard"

    world = bpy.data.worlds.new("World")
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.10, 0.06, 0.16, 1.0)  # violet ambient fill
    bg.inputs[1].default_value = 1.0

    cam_data = bpy.data.cameras.new("Camera")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = ortho_scale
    cam = bpy.data.objects.new("Camera", cam_data)
    sc.collection.objects.link(cam)
    tilt = math.radians(30.0)
    dist = 10.0
    cam.location = (0.0, -dist * math.sin(tilt), target_z + dist * math.cos(tilt))
    cam.rotation_euler = (tilt, 0.0, 0.0)
    sc.camera = cam

    sun_data = bpy.data.lights.new("Key", type="SUN")
    sun_data.energy = 3.0
    sun_data.angle = math.radians(10)
    sun = bpy.data.objects.new("Key", sun_data)
    sun.rotation_euler = (math.radians(35), 0.0, math.radians(-35))
    sc.collection.objects.link(sun)


def render(out_dir, key):
    path = os.path.join(out_dir, key + ".png")
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("[render_props] wrote", path)


# ---------------------------------------------------------------------------------
# Props (1 Blender unit = 1 game tile). All original, built from primitives.
# ---------------------------------------------------------------------------------
def build_wardrobe():
    wood = material("wood", (64, 44, 48))
    dark = material("dark", (30, 20, 24))
    brass = material("brass", (170, 150, 100))
    box("body", (0.86, 0.5, 1.45), (0, 0, 0.725), wood)
    box("cap", (0.92, 0.54, 0.06), (0, 0, 1.47), material("cap", (80, 58, 62)))
    box("split", (0.02, 0.02, 1.25), (0, -0.255, 0.72), dark)
    box("h1", (0.04, 0.03, 0.1), (-0.06, -0.26, 0.8), brass)
    box("h2", (0.04, 0.03, 0.1), (0.06, -0.26, 0.8), brass)
    return 64, 96, 1.62, 0.72


def build_cabinet():
    body = material("body", (66, 50, 58))
    drawer = material("drawer", (52, 38, 46))
    brass = material("brass", (170, 150, 100))
    box("body", (0.86, 0.55, 0.62), (0, 0, 0.31), body)
    box("top", (0.9, 0.58, 0.04), (0, 0, 0.63), material("top", (92, 72, 80)))
    for i, z in enumerate((0.43, 0.2)):
        box("d%d" % i, (0.76, 0.02, 0.17), (0, -0.28, z), drawer)
        box("k%d" % i, (0.06, 0.03, 0.04), (0, -0.3, z), brass)
    return 64, 64, 1.12, 0.35


def build_crate():
    wood = material("wood", (92, 76, 58))
    dark = material("dark", (62, 50, 38))
    light = material("light", (116, 98, 74))
    box("body", (0.8, 0.8, 0.66), (0, 0, 0.33), wood)
    for sx in (-0.39, 0.39):
        for sy in (-0.39, 0.39):
            box("post", (0.06, 0.06, 0.68), (sx, sy, 0.34), dark)
    box("lid1", (0.8, 0.12, 0.02), (0, -0.2, 0.67), light)
    box("lid2", (0.8, 0.12, 0.02), (0, 0.2, 0.67), light)
    return 64, 64, 1.12, 0.35


def build_chair():
    wood = material("wood", (86, 64, 56))
    dark = material("dark", (50, 36, 32))
    box("seat", (0.5, 0.5, 0.07), (0, 0, 0.45), wood)
    for sx in (-0.21, 0.21):
        for sy in (-0.21, 0.21):
            box("leg", (0.05, 0.05, 0.45), (sx, sy, 0.22), dark)
    box("back", (0.5, 0.06, 0.5), (0, 0.22, 0.73), material("back", (70, 52, 46)))
    return 64, 64, 1.12, 0.45


def build_table():
    wood = material("wood", (80, 60, 52))
    dark = material("dark", (50, 36, 32))
    cup = material("cup", (190, 184, 176))
    box("top", (0.92, 0.62, 0.06), (0, 0, 0.62), wood)
    for sx in (-0.4, 0.4):
        for sy in (-0.25, 0.25):
            box("leg", (0.06, 0.06, 0.6), (sx, sy, 0.3), dark)
    cylinder("cup1", 0.07, 0.1, (-0.22, -0.05, 0.7), cup)
    cylinder("cup2", 0.07, 0.1, (0.2, 0.08, 0.7), cup)
    return 64, 64, 1.12, 0.4


def build_bed():
    frame = material("frame", (54, 40, 44))
    sheet = material("sheet", (180, 176, 170))
    blanket = material("blanket", (70, 52, 90))
    box("frame", (0.9, 0.95, 0.25), (0, 0, 0.125), frame)
    box("mattress", (0.84, 0.9, 0.12), (0, 0, 0.31), sheet)
    box("pillow", (0.6, 0.22, 0.08), (0, 0.3, 0.41), sheet)
    box("blanket", (0.86, 0.58, 0.06), (0, -0.15, 0.39), blanket)
    box("head", (0.9, 0.06, 0.5), (0, 0.46, 0.25), frame)
    return 64, 64, 1.12, 0.25


def build_fusebox():
    metal = material("metal", (88, 92, 96))
    inner = material("inner", (58, 62, 68))
    fuse = material("fuse", (150, 140, 110))
    box("case", (0.6, 0.3, 0.8), (0, 0.15, 0.45), metal)
    box("panel", (0.5, 0.02, 0.68), (0, -0.01, 0.45), inner)
    for z in (0.62, 0.45, 0.28):
        box("fuse", (0.36, 0.04, 0.08), (0, -0.03, z), fuse)
    return 64, 64, 1.12, 0.45


def build_shelf():
    wood = material("wood", (52, 40, 40))
    books = [material("b%d" % i, c) for i, c in
             enumerate(((84, 40, 52), (60, 70, 62), (80, 70, 96), (110, 100, 86)))]
    box("body", (1.0, 0.45, 1.1), (0, 0, 0.55), wood)
    for row, z in enumerate((0.85, 0.5, 0.18)):
        for i in range(8):
            h = 0.22 + 0.04 * ((i * 7 + row) % 3)
            box("book", (0.1, 0.06, h), (-0.42 + i * 0.12, -0.23, z - (0.26 - h) / 2), books[(i + row) % 4])
    return 64, 64, 1.12, 0.5


def build_tenant():
    black = material("black", (10, 8, 14))
    shade = material("shade", (26, 18, 34))
    eye = material("eye", (210, 40, 52), emission=6.0)
    box("legL", (0.07, 0.07, 0.8), (-0.08, 0, 0.4), black)
    box("legR", (0.07, 0.07, 0.8), (0.08, 0, 0.4), black)
    box("torso", (0.26, 0.14, 0.55), (0, 0, 1.08), black)
    box("shoulders", (0.42, 0.14, 0.1), (0, 0, 1.33), shade)
    box("armL", (0.05, 0.05, 0.95), (-0.24, 0, 0.88), black)  # arms far too long
    box("armR", (0.05, 0.05, 0.95), (0.24, 0, 0.88), black)
    box("neck", (0.06, 0.06, 0.12), (0, 0, 1.43), black)
    head = sphere("head", 0.13, (0.03, 0, 1.6), black)
    head.rotation_euler = (0, math.radians(18), 0)  # tilted, like it is listening
    box("eyes", (0.14, 0.02, 0.025), (0.03, -0.125, 1.61), eye)
    return 64, 96, 2.0, 0.95


PROPS = {
    "wardrobe": build_wardrobe,
    "cabinet": build_cabinet,
    "crate": build_crate,
    "chair": build_chair,
    "table": build_table,
    "bed": build_bed,
    "fusebox": build_fusebox,
    "shelf": build_shelf,
    "tenant": build_tenant,
}


def main():
    out_dir = output_dir()
    os.makedirs(out_dir, exist_ok=True)
    wanted = only_list()
    for key, builder in PROPS.items():
        if wanted and key not in wanted:
            continue
        reset()
        res_x, res_y, scale, target_z = builder()
        setup(res_x, res_y, scale, target_z)
        render(out_dir, key)
    print("[render_props] done ->", out_dir)


if __name__ == "__main__":
    main()
