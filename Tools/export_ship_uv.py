"""Writes the ship mesh's triangles (position + UV per corner) to Saved/ShipArt/<mesh>_uv.json.

make_running_light_masks.py uses it to work out where on the hull a texel is.
Run as an editor commandlet, like import_ship_art.py.
"""

import json
import os

import unreal

MESH = "/Game/Art/Ships/SPCSH_Green/SM_SPCSH_Green"

mesh = unreal.load_asset(MESH)
description = mesh.get_static_mesh_description(0)
triangles = []
for index in range(description.get_triangle_count()):
    corners = []
    for instance in description.get_triangle_vertex_instances(unreal.TriangleID(index)):
        position = description.get_vertex_position(description.get_vertex_instance_vertex(instance))
        uv = description.get_vertex_instance_uv(instance, 0)
        corners.append([position.x, position.y, position.z, uv.x, uv.y])
    triangles.append(corners)

out_dir = os.path.join(unreal.Paths.project_saved_dir(), "ShipArt")
os.makedirs(out_dir, exist_ok=True)
out_file = os.path.join(out_dir, os.path.basename(MESH) + "_uv.json")
with open(out_file, "w") as handle:
    json.dump(triangles, handle)
unreal.log("SHIPART wrote %d triangles to %s" % (len(triangles), out_file))
