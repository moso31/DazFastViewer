"""后台 Blender 渲染 DBZ / 原生几何对照；两侧共用光照、相机和材质。"""
import argparse
import gzip
import json
import sys
from pathlib import Path
from urllib.parse import unquote

import bpy
import numpy as np
from mathutils import Vector


def read(path):
    data = Path(path).read_bytes()
    return json.loads(gzip.decompress(data) if data[:2] == b"\x1f\x8b" else data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("dbz", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--head", action="store_true")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    native, dbz = read(args.native), read(args.dbz)
    source = read(native["input"])
    nodes = {n["id"]: n for n in source["scene"]["nodes"]}
    roots = [Path(p) for p in ("H:/G1", "H:/G3", "C:/Users/Public/Documents/My DAZ 3D Library",
                               "C:/Users/xatia/Documents/DAZ 3D/Studio/My Library")]
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    all_positions = []
    separation = 0.7 if not args.head else 0.65
    for obj in native["objects"]:
        node = nodes[obj["id"].split("/")[0]]
        file, geometry_id = unquote(node["geometries"][0]["url"]).split("#")
        document = read(next(p / file.lstrip("/") for p in roots if (p / file.lstrip("/")).is_file())) if file else source
        geometry = next(g for g in document["geometry_library"] if g["id"] == geometry_id)
        polygons = geometry["polylist"]["values"]
        faces = [p[2:] for p in polygons]
        expected = next(f for f in dbz["figures"] if f.get("label") == obj["label"])
        ref = np.asarray(expected["vertices"])[:, [0, 2, 1]] * [0.01, -0.01, 0.01]
        for mode, positions, shift in (("DBZ", ref, separation / 2),
                                       ("Native", np.asarray(obj["world"]), -separation / 2)):
            positions = positions + [shift, 0, 0]
            all_positions.append(positions)
            mesh = bpy.data.meshes.new(mode + obj["label"])
            mesh.from_pydata(positions.tolist(), [], faces)
            mesh.update()
            ob = bpy.data.objects.new(mesh.name, mesh)
            bpy.context.collection.objects.link(ob)
            for name in geometry["polygon_material_groups"]["values"]:
                color = (0.55, 0.37, 0.29, 1)
                if "Skirt" in obj["label"]:
                    color = (0.24, 0.33, 0.17, 1) if "LSO" in obj["label"] else (0.12, 0.15, 0.18, 1)
                elif "Blouse" in obj["label"]:
                    color = (0.7, 0.7, 0.7, 1)
                elif "Sweatshirt" in obj["label"]:
                    color = (0.06, 0.065, 0.07, 1)
                elif name in ("Pupils", "Irises"):
                    color = (0.025, 0.03, 0.035, 1)
                elif name in ("Sclera", "Teeth"):
                    color = (0.8, 0.8, 0.75, 1)
                mat = bpy.data.materials.new(mode + obj["label"] + name)
                mat.diffuse_color = color
                mesh.materials.append(mat)
            for poly, original in zip(mesh.polygons, polygons):
                poly.material_index = original[1]
                poly.use_smooth = True
            # 比较原始几何；两侧同样使用平滑法线，不细分、不修正顶点。
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.studiolight_rotate_z = 0.4
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = "BOTH"
    scene.display.shading.background_type = "WORLD"
    scene.world.color = (0.16, 0.16, 0.16)
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000 if not args.head else 500
    scene.render.resolution_percentage = 100
    bpy.ops.object.camera_add(location=(-1.25, 2.0, 1.2 if not args.head else 1.13))
    camera = bpy.context.object
    target = Vector((-1.25, -1.0, 0.63 if not args.head else 1.085))
    camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 1.65 if not args.head else 1.3
    scene.camera = camera
    scene.render.filepath = str(args.output.resolve())
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    main()
