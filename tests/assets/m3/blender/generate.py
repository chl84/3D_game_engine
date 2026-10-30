"""Generate the M3 GLB fixtures with Blender.

Run with:
    blender -b --factory-startup --python tests/assets/m3/blender/generate.py

The generated files are checked in so the unit tests do not need Blender.
"""

from __future__ import annotations

import math
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
OUTPUT = HERE.parent


def make_checker_image() -> bpy.types.Image:
    image = bpy.data.images.new("M3BaseColor", width=4, height=4, alpha=True)
    pixels = []
    for y in range(4):
        for x in range(4):
            if (x + y) % 2 == 0:
                pixels.extend([0.85, 0.20, 0.10, 1.0])
            else:
                pixels.extend([0.95, 0.90, 0.80, 1.0])
    image.pixels = pixels
    image.filepath_raw = str(OUTPUT / "m3_basecolor.png")
    image.file_format = "PNG"
    image.save()
    image.pack()
    return image


def add_empty(name: str, parent: bpy.types.Object | None) -> bpy.types.Object:
    empty = bpy.data.objects.new(name, None)
    empty.empty_display_type = "PLAIN_AXES"
    empty.empty_display_size = 0.25
    bpy.context.scene.collection.objects.link(empty)
    if parent is not None:
        empty.parent = parent
    return empty


def build_scene() -> bpy.types.Object:
    bpy.ops.wm.read_factory_settings(use_empty=True)

    image = make_checker_image()
    material = bpy.data.materials.new("M3Painted")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    principled = nodes["Principled BSDF"]
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    material.node_tree.links.new(texture.outputs["Color"], principled.inputs["Base Color"])
    principled.inputs["Metallic"].default_value = 0.25
    principled.inputs["Roughness"].default_value = 0.6
    principled.inputs["Emission Color"].default_value = (0.05, 0.02, 0.01, 1.0)
    principled.inputs["Emission Strength"].default_value = 1.0

    root = add_empty("Root", None)
    pivot = add_empty("Pivot", root)
    pivot.location = (1.5, 0.0, 0.25)
    pivot.rotation_euler = (0.0, 0.0, math.radians(30.0))

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, 1.0, -2.0))
    cube = bpy.context.active_object
    cube.name = "Cube"
    cube.data.name = "CubeMesh"
    cube.parent = pivot
    cube.rotation_euler = (math.radians(20.0), 0.0, 0.0)
    cube.scale = (0.5, 0.75, 1.25)
    cube.data.materials.append(material)
    return pivot


def export(path: Path, animations: bool) -> None:
    bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=False,
        export_animations=animations,
        export_apply=True,
        export_yup=True,
        export_texcoords=True,
        export_normals=True,
        export_tangents=False,
        export_materials="EXPORT",
        export_image_format="AUTO",
        export_cameras=False,
        export_lights=False,
        export_skins=False,
        export_morph=False,
        export_draco_mesh_compression_enable=False,
        export_meshopt_compression_enable=False,
    )


def main() -> None:
    pivot = build_scene()
    export(OUTPUT / "blender_cube.glb", animations=False)

    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = 20
    pivot.rotation_mode = "XYZ"
    pivot.rotation_euler[1] = 0.0
    pivot.keyframe_insert(data_path="rotation_euler", frame=1)
    pivot.rotation_euler[1] = math.radians(45.0)
    pivot.keyframe_insert(data_path="rotation_euler", frame=20)
    export(OUTPUT / "blender_animated.glb", animations=True)


if __name__ == "__main__":
    main()
