"""Create the original high-poly normal-mapped machined-ring regression (MIT).

blender -b --factory-startup -noaudio -t 2 --python \
  tests/assets/m3/blender/generate_large_normalmap.py -- --output-dir /tmp/m3-large
"""
import argparse
import sys
from pathlib import Path

import bpy

parser = argparse.ArgumentParser()
parser.add_argument("--output-dir", type=Path, required=True)
out = parser.parse_args(sys.argv[sys.argv.index("--") + 1:]).output_dir
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_torus_add(
    major_segments=256, minor_segments=128, major_radius=2.0, minor_radius=0.5,
    generate_uvs=True,
)
obj = bpy.context.object
obj.name = "HighPolyMachinedRing"
for polygon in obj.data.polygons:
    polygon.use_smooth = True
subdivision = obj.modifiers.new("SmoothHighPolySurface", "SUBSURF")
subdivision.levels = 1
bpy.ops.object.modifier_apply(modifier=subdivision.name)
modifier = obj.modifiers.new("ExportTriangles", "TRIANGULATE")
bpy.ops.object.modifier_apply(modifier=modifier.name)
assert len(obj.data.polygons) == 262144

material = bpy.data.materials.new("BrushedMetalWithBakedNormal")
material.use_nodes = True
obj.data.materials.append(material)
nodes, links = material.node_tree.nodes, material.node_tree.links
bsdf = nodes.get("Principled BSDF")
bsdf.inputs["Base Color"].default_value = (0.32, 0.38, 0.45, 1)
bsdf.inputs["Metallic"].default_value = 0.8
bsdf.inputs["Roughness"].default_value = 0.35
noise = nodes.new("ShaderNodeTexNoise")
noise.inputs["Scale"].default_value = 32
noise.inputs["Detail"].default_value = 2
bump = nodes.new("ShaderNodeBump")
bump.inputs["Distance"].default_value = 0.015
links.new(noise.outputs["Fac"], bump.inputs["Height"])
links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
image = bpy.data.images.new("RingBakedNormal", width=256, height=256, alpha=False)
image.colorspace_settings.name = "Non-Color"
texture = nodes.new("ShaderNodeTexImage")
texture.image = image
nodes.active = texture
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 1
scene.render.bake.normal_space = "TANGENT"
scene.render.bake.margin = 4
bpy.ops.object.bake(type="NORMAL")
nodes.remove(bump)
nodes.remove(noise)
normal = nodes.new("ShaderNodeNormalMap")
links.new(texture.outputs["Color"], normal.inputs["Color"])
links.new(normal.outputs["Normal"], bsdf.inputs["Normal"])
image.pack()
# Neutral relative image metadata; no machine-specific path in the GLB.
image.filepath_raw = "//ring_baked_normal.png"
bpy.ops.export_scene.gltf(
    filepath=str(out / "blender_large_normal_no_tangents.glb"),
    export_format="GLB", export_animations=False, export_apply=True,
    export_normals=True, export_texcoords=True, export_tangents=False,
    export_cameras=False, export_lights=False, export_skins=False,
    export_morph=False, export_draco_mesh_compression_enable=False,
    export_meshopt_compression_enable=False,
)
