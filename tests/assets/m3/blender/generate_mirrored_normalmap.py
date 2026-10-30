"""Create the original mirrored-UV normal-map fixture using local Blender.

Run with Blender 5.2.2 LTS:
    blender -b --factory-startup -noaudio -t 2 --python <this-file> -- --output-dir <directory>
Generate into a neutral temporary directory, then copy the four output assets
into tests/assets/m3. No external models, textures or add-ons are used.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 1
    scene.render.threads_mode = "FIXED"
    scene.render.threads = 2
    scene.render.bake.normal_space = "TANGENT"
    scene.render.bake.margin = 6

    # A curved calibration coupon with a central UV seam. The right-hand
    # island has opposite orientation, occupies distinct texels and never
    # overlaps the left island. Both orientations must be baked separately.
    nx, ny = 32, 16
    vertices = []
    for y in range(ny + 1):
        v = 2 * y / ny - 1
        for x in range(nx + 1):
            u = 2 * x / nx - 1
            z = 0.18 * math.cos(u * math.pi / 2) * math.cos(v * math.pi / 2)
            vertices.append((1.6 * u, v, z))
    faces = []
    for y in range(ny):
        for x in range(nx):
            a = y * (nx + 1) + x
            b, c, d = a + 1, a + nx + 2, a + nx + 1
            faces.extend(((a, b, c), (a, c, d)))
    mesh = bpy.data.meshes.new("MirroredCouponMesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    coupon = bpy.data.objects.new("MirroredNormalCoupon", mesh)
    scene.collection.objects.link(coupon)
    uv = mesh.uv_layers.new(name="UVMap")
    orientations = {"positive": 0, "negative": 0}
    for face in mesh.polygons:
        face.use_smooth = True
        right = sum(mesh.vertices[i].co.x for i in face.vertices) > 0
        corners = []
        for loop in face.loop_indices:
            point = mesh.vertices[mesh.loops[loop].vertex_index].co
            x = point.x / 1.6
            u = 0.54 + 0.42 * (1 - x) if right else 0.04 + 0.42 * (x + 1)
            value = (u, 0.06 + 0.88 * (point.y + 1) / 2)
            uv.data[loop].uv = value
            corners.append(value)
        a, b, c = corners
        area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        assert abs(area) > 1e-8
        orientations["positive" if area > 0 else "negative"] += 1
    assert orientations == {"positive": 512, "negative": 512}

    source = bpy.data.materials.new("Coupon_Bake_Source")
    source.use_nodes = True
    source.use_fake_user = True
    nodes, links = source.node_tree.nodes, source.node_tree.links
    bsdf = nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (0.42, 0.24, 0.07, 1)
    bsdf.inputs["Roughness"].default_value = 0.6
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 16
    noise.inputs["Detail"].default_value = 1
    wave = nodes.new("ShaderNodeTexWave")
    wave.bands_direction = "DIAGONAL"
    wave.inputs["Scale"].default_value = 7
    wave.inputs["Distortion"].default_value = 0.6
    mix = nodes.new("ShaderNodeMath")
    mix.operation = "ADD"
    links.new(noise.outputs["Fac"], mix.inputs[0])
    links.new(wave.outputs["Fac"], mix.inputs[1])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Distance"].default_value = 0.035
    bump.inputs["Strength"].default_value = 0.7
    links.new(mix.outputs[0], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    image = bpy.data.images.new("MirroredCouponNormal", width=256, height=256, alpha=False)
    image.colorspace_settings.name = "Non-Color"
    target = nodes.new("ShaderNodeTexImage")
    target.image = image
    nodes.active = target
    coupon.data.materials.append(source)
    bpy.context.view_layer.objects.active = coupon
    coupon.select_set(True)
    bpy.ops.object.bake(type="NORMAL")
    pixels = list(image.pixels)
    assert max(pixels[0::4]) - min(pixels[0::4]) > 0.1
    assert max(pixels[1::4]) - min(pixels[1::4]) > 0.1
    image.filepath_raw = str(out / "blender_mirrored_normal.png")
    image.file_format = "PNG"
    image.save()
    image.pack()
    # Retain the procedural bake recipe as a separate material in the .blend.
    baked = source.copy()
    baked.name = "Coupon_Baked_Normal"
    baked.use_fake_user = False
    baked_nodes, baked_links = baked.node_tree.nodes, baked.node_tree.links
    for node in list(baked_nodes):
        if node.type not in {"BSDF_PRINCIPLED", "OUTPUT_MATERIAL", "TEX_IMAGE"}:
            baked_nodes.remove(node)
    normal = baked_nodes.new("ShaderNodeNormalMap")
    baked_links.new(next(n for n in baked_nodes if n.type == "TEX_IMAGE").outputs["Color"], normal.inputs["Color"])
    baked_links.new(normal.outputs["Normal"], baked_nodes["Principled BSDF"].inputs["Normal"])
    coupon.data.materials[0] = baked
    coupon["fixture_license"] = "MIT"
    coupon["bake_recipe_material"] = source.name

    # A simple machined base gives the calibration surface a readable silhouette.
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -0.14))
    base = bpy.context.object
    base.name = "CouponBase"
    base.dimensions = (3.38, 2.18, 0.22)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel = base.modifiers.new("MachinedEdges", "BEVEL")
    bevel.width, bevel.segments = 0.06, 3
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    triangulate = base.modifiers.new("FixedTriangles", "TRIANGULATE")
    bpy.ops.object.modifier_apply(modifier=triangulate.name)
    metal = bpy.data.materials.new("BaseMetal")
    metal.use_nodes = True
    metal.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.12, 0.16, 0.20, 1)
    metal.node_tree.nodes["Principled BSDF"].inputs["Metallic"].default_value = 0.7
    metal.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.35
    base.data.materials.append(metal)
    fixture_meshes = (coupon, base)
    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    for obj in fixture_meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = coupon
    for tangents in (False, True):
        filename = "blender_mirrored_tangents.glb" if tangents else "blender_mirrored_no_tangents.glb"
        bpy.ops.export_scene.gltf(
            filepath=str(out / filename), export_format="GLB", use_selection=True,
            export_animations=False, export_apply=True, export_yup=True,
            export_normals=True, export_texcoords=True, export_tangents=tangents,
            export_cameras=False, export_lights=False, export_skins=False,
            export_morph=False, export_extras=False,
            export_draco_mesh_compression_enable=False,
            export_meshopt_compression_enable=False,
        )

    # Lights/camera belong only to the editable Blender source, not the GLBs.
    bpy.ops.object.camera_add(location=(3.7, -4.5, 4.6))
    camera = bpy.context.object
    camera.name = "InspectionCamera"
    camera.rotation_euler = (Vector((0, 0, 0)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 5.0
    scene.camera = camera
    for name, location, energy, size in (
        ("InspectionKey", (0, -2, 4), 500, 4),
        ("InspectionFill", (3, 3, 2), 250, 3),
    ):
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.object
        light.name = name
        light.data.energy, light.data.shape, light.data.size = energy, "DISK", size
        light.rotation_euler = (-light.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.resolution_x, scene.render.resolution_y = 640, 480
    scene.render.resolution_percentage = 100
    scene.cycles.samples = 16
    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    coupon.select_set(True)
    bpy.context.view_layer.objects.active = coupon
    # Packed image and relative path keep the source portable. The save location
    # is deliberately neutral rather than a developer's personal home directory.
    image.filepath_raw = "//blender_mirrored_normal.png"
    # Factory workspaces can retain a file-browser directory pointing to the
    # current user's home, even when no external data is used. Clear it before
    # saving a binary fixture intended for the public repository.
    for screen in bpy.data.screens:
        for area in screen.areas:
            for space in area.spaces:
                if space.type == "FILE_BROWSER" and space.params is not None:
                    space.params.directory = str(out).encode()
    bpy.ops.wm.save_as_mainfile(filepath=str(out / "blender_mirrored_normalmap.blend"), compress=False, relative_remap=True)
    print("FIXTURE " + json.dumps({
        "blender": bpy.app.version_string, "meshObjects": len(fixture_meshes),
        "couponTriangles": len(mesh.polygons), "uvOrientations": orientations,
        "normalMap": [256, 256], "license": "MIT",
    }, sort_keys=True))


if __name__ == "__main__":
    main()
