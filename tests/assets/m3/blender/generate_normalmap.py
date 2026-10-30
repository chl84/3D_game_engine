"""Bake an original normal map and export matching GLBs with/without tangents.

Run: blender -b --factory-startup --python tests/assets/m3/blender/generate_normalmap.py
The CPU tangent regression uses both files; M3.5 uses them for PBR golden images.
"""
from pathlib import Path
import bpy

out = Path(__file__).resolve().parent.parent
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=1.0)
obj = bpy.context.object
obj.name = "BakedNormalSphere"
for poly in obj.data.polygons:
    poly.use_smooth = True
modifier = obj.modifiers.new("FixedTriangles", "TRIANGULATE")
bpy.ops.object.modifier_apply(modifier=modifier.name)
material = bpy.data.materials.new("BakedNormalMaterial")
material.use_nodes = True
obj.data.materials.append(material)
nodes = material.node_tree.nodes
links = material.node_tree.links
bsdf = nodes.get("Principled BSDF")
bsdf.inputs["Base Color"].default_value = (0.45, 0.25, 0.08, 1.0)
bsdf.inputs["Metallic"].default_value = 0.0
bsdf.inputs["Roughness"].default_value = 0.65
noise = nodes.new("ShaderNodeTexNoise")
noise.inputs["Scale"].default_value = 7.0
noise.inputs["Detail"].default_value = 1.0
bump = nodes.new("ShaderNodeBump")
bump.inputs["Distance"].default_value = 0.08
links.new(noise.outputs["Fac"], bump.inputs["Height"])
links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
image = bpy.data.images.new("BakedNormal", width=64, height=64, alpha=False)
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
image.filepath_raw = str(out / "blender_baked_normal.png")
image.file_format = "PNG"
image.save()
image.pack()
for tangents in (False, True):
    bpy.ops.export_scene.gltf(
        filepath=str(out / ("blender_normal_tangents.glb" if tangents else "blender_normal_no_tangents.glb")),
        export_format="GLB", export_animations=False, export_apply=True,
        export_normals=True, export_texcoords=True, export_tangents=tangents,
        export_cameras=False, export_lights=False, export_skins=False,
        export_morph=False, export_draco_mesh_compression_enable=False,
        export_meshopt_compression_enable=False,
    )
