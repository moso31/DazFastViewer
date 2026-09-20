"""在独立 Blender 进程中生成 DAZ 材质参考，不保存用户偏好。"""
import argparse
import hashlib
import json
import math
import sys
import tomllib
from pathlib import Path

import addon_utils
import bpy
import numpy as np
from mathutils import Matrix, Vector


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def value(item):
    if isinstance(item, (str, int, float, bool)) or item is None:
        return item
    try:
        return [value(x) for x in item]
    except TypeError:
        return str(item)


def matrix(elements):
    return Matrix([elements[k:k + 4] for k in range(0, 12, 4)] + [[0, 0, 0, 1]])


def tree_report(tree, seen=None):
    seen = set() if seen is None else seen
    if not tree or tree.name in seen:
        return None
    seen.add(tree.name)
    nodes = []
    for node in tree.nodes:
        item = {"name": node.name, "type": node.bl_idname, "label": node.label,
                "inputs": [{"name": s.name, "identifier": s.identifier, "linked": s.is_linked,
                            "value": value(getattr(s, "default_value", None))} for s in node.inputs],
                "outputs": [{"name": s.name, "identifier": s.identifier,
                             "value": value(getattr(s, "default_value", None))} for s in node.outputs]}
        for key in ("operation", "blend_type", "data_type", "factor_mode", "clamp_factor", "clamp_result", "space", "uv_map", "invert", "interpolation", "extension", "distribution", "subsurface_method"):
            if hasattr(node, key):
                item[key] = value(getattr(node, key))
        if getattr(node, "image", None):
            image = node.image
            path = Path(bpy.path.abspath(image.filepath))
            item["image"] = {"file": str(path), "colorspace": image.colorspace_settings.name,
                             "exists": path.is_file(), "size": list(image.size)}
            if not path.is_file():
                raise RuntimeError(f"参考材质缺少贴图：{path}")
        if getattr(node, "node_tree", None):
            item["group"] = tree_report(node.node_tree, seen.copy())
        nodes.append(item)
    return {"name": tree.name, "nodes": nodes,
            "links": [{"from_node": l.from_node.name, "from_socket": l.from_socket.identifier,
                       "to_node": l.to_node.name, "to_socket": l.to_socket.identifier} for l in tree.links]}


def imported_reference(args):
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    module = addon_utils.enable("bl_ext.user_default.import_daz", default_set=True, persistent=False)
    if module is None:
        raise RuntimeError("无法启用已安装的 DAZ Importer")
    settings = {"contentDirs": [str(p.resolve()) for p in args.content_root], "cloudDirs": [],
                "scale": 0.01, "zup": True, "unflipped": False, "silentMode": True,
                "rememberLastFolder": False, "materialMethod": "SELECT", "useBump": True,
                "useNormalMap": True, "useDisplacement": False, "useEmission": True,
                "useVolume": True, "useDazImages": True, "useUnusedTextures": False,
                "useFakeCaustics": False, "bumpMultiplier": 1.0, "imageInterpolation": "Linear",
                "sssMethod": "BURLEY_SKIN", "skinMethod": "SSS", "onStrengthAdjusters": "NONE",
                "useSimplifiedCoat": False, "usePruneNodes": True}
    for key, setting in settings.items():
        if module.get_global_setting(key) is None:
            raise RuntimeError(f"Importer API 不支持设置 {key}")
        module.set_global_setting(key, setting)
        if module.get_global_setting(key) != setting:
            raise RuntimeError(f"Importer 设置未生效：{key}")
    result = bpy.ops.daz.import_daz_manually(filepath=str(args.file.resolve()),
                                           directory=str(args.file.resolve().parent), files=[{"name": args.file.name}], fitMeshes="UNIQUE",
                                           materialMethod="EXTENDED_PRINCIPLED")
    if "FINISHED" not in result:
        raise RuntimeError(f"DAZ 导入失败：{result}")
    bpy.context.view_layer.update()
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        raise RuntimeError("DAZ Importer 未产生网格")
    plugin_manifest = tomllib.loads(Path(module.__file__).with_name("blender_manifest.toml").read_text("utf-8"))
    report = {"blender": bpy.app.version_string, "blender_hash": bpy.app.build_hash.decode(),
              "reference_script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "importer_version": plugin_manifest["version"], "importer_file": module.__file__,
              "importer_init_sha256": hashlib.sha256(Path(module.__file__).read_bytes()).hexdigest(),
              "input": str(args.file.resolve()), "settings": settings,
              "fit_meshes": "UNIQUE", "material_method": "EXTENDED_PRINCIPLED",
              "geometry_policy": "验证导入基础顶点后，材质对照共用 Runtime 三角形和逐角 UV；不应用 Modifier",
              "objects": [{"name": o.name, "vertices": len(o.data.vertices), "polygons": len(o.data.polygons),
                           "matrix": value(o.matrix_world), "materials": [m.name if m else None for m in o.data.materials],
                           "modifiers": [{"name": m.name, "type": m.type} for m in o.modifiers]} for o in meshes],
              "materials": [{"name": m.name, "custom_properties": {k: value(m[k]) for k in m.keys()},
                             "graph": tree_report(m.node_tree)} for m in sorted({m for o in meshes for m in o.data.materials if m}, key=lambda m: m.name)]}
    write(args.output / "reference-materials.json", report)
    return meshes, report


def construct_scene(contract, imported):
    checks = []
    matches = {}
    uv_names = {}
    for instance in contract["instances"]:
        mesh = contract["meshes"][instance["mesh"]]
        if instance["id"] == "preview-floor":
            continue
        transform = matrix(instance["transform"])
        positions = np.array([transform @ Vector(v) for v in mesh["positions"]])
        candidates = []
        for obj in imported:
            if len(obj.data.vertices) == len(positions):
                actual = np.array([obj.matrix_world @ v.co for v in obj.data.vertices])
                error = float(np.max(np.linalg.norm(actual - positions, axis=1)))
                candidates.append((error, obj))
        if not candidates:
            raise RuntimeError(f"没有顶点数匹配的参考网格：{instance['id']}")
        error, obj = min(candidates, key=lambda p: p[0])
        if error > 1e-4:
            raise RuntimeError(f"基础顶点不一致：{instance['id']}，最大误差 {error} 米")
        materials = {}
        for slot, name in enumerate(mesh["material_slots"]):
            semantic_id = contract["materials"][instance["materials"][slot]]["id"]
            found = [m for m in obj.data.materials if m and (m.name in (name, semantic_id) or m.name.split(".")[0] == name
                     or m.get("DazMatName") == name)]
            if len(found) != 1:
                raise RuntimeError(f"材质槽无法唯一匹配：{name}；参考 {[m.name for m in obj.data.materials if m]}")
            materials[slot] = found[0]
        # 每个三角形必须位于参考导入的同组多边形内，防止仅名称一致而实际绑定错误。
        faces = {}
        for p in obj.data.polygons:
            for vertex in p.vertices:
                faces.setdefault(vertex, []).append(p)
        uv_error = 0.0
        active_uv = obj.data.uv_layers.active
        if active_uv is None:
            raise RuntimeError(f"参考网格没有 UV：{obj.name}")
        for triangle in mesh["triangles"]:
            vertices = set(triangle["vertices"])
            compatible = [p for p in faces.get(triangle["vertices"][0], []) if vertices.issubset(p.vertices)
                          and obj.data.materials[p.material_index] == materials[triangle["material_slot"]]]
            if not compatible:
                raise RuntimeError(f"参考多边形或材质绑定不一致：{instance['id']}")
            polygon = compatible[0]
            corners = {v: active_uv.data[loop].uv for v, loop in zip(polygon.vertices, polygon.loop_indices)}
            uv_error = max(uv_error, max((corners[v] - Vector(uv)).length for v, uv in zip(triangle["vertices"], triangle["uv"])))
        if uv_error > 1e-5:
            raise RuntimeError(f"参考 UV 不一致：{instance['id']}，最大误差 {uv_error}")
        checks.append({"instance": instance["id"], "reference_object": obj.name,
                       "vertices": len(positions), "max_vertex_error_m": error,
                       "max_uv_error": uv_error, "uv_layer": active_uv.name,
                       "material_slots": {mesh["material_slots"][i]: m.name for i, m in materials.items()},
                       "triangle_material_binding": "PASS"})
        matches[instance["id"]] = materials
        uv_names[instance["id"]] = active_uv.name
    for obj in list(bpy.context.scene.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for instance in contract["instances"]:
        source = contract["meshes"][instance["mesh"]]
        mesh = bpy.data.meshes.new(instance["id"])
        mesh.from_pydata(source["positions"], [], [t["vertices"] for t in source["triangles"]])
        mesh.update()
        if instance["id"] == "preview-floor":
            semantic = contract["materials"][instance["materials"][0]]
            mat = bpy.data.materials.new("reference-floor")
            mat.use_nodes = True
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            bsdf.inputs["Base Color"].default_value = (*semantic["base_color"], 1)
            bsdf.inputs["Roughness"].default_value = semantic["roughness"]
            mesh.materials.append(mat)
        else:
            for slot in range(len(source["material_slots"])):
                mesh.materials.append(matches[instance["id"]][slot])
        uv = mesh.uv_layers.new(name=uv_names.get(instance["id"], "UVMap"))
        for polygon, triangle in zip(mesh.polygons, source["triangles"]):
            polygon.material_index = triangle["material_slot"]
            polygon.use_smooth = source["smooth"]
            for loop, coordinates in zip(polygon.loop_indices, triangle["uv"]):
                uv.data[loop].uv = coordinates
        obj = bpy.data.objects.new(instance["id"], mesh)
        bpy.context.collection.objects.link(obj)
        obj.matrix_world = matrix(instance["transform"])
    scene = bpy.context.scene
    camera = bpy.data.cameras.new("reference-camera")
    obj = bpy.data.objects.new(camera.name, camera)
    scene.collection.objects.link(obj)
    transform = matrix(contract["camera"]["transform"])
    for row in range(3):
        transform[row][2] *= -1
    obj.matrix_world = transform
    width, height = contract["camera"]["width"], contract["camera"]["height"]
    camera.sensor_fit = "VERTICAL" if width >= height else "HORIZONTAL"
    camera.sensor_height = camera.sensor_width = 36
    camera.lens = 18 / math.tan(contract["camera"]["fov_short_axis"] / 2)
    camera.clip_start = 0.00001
    scene.camera = obj
    for source in contract["lights"]:
        light = bpy.data.lights.new(source["id"], "AREA")
        light.shape = "RECTANGLE"
        light.size, light.size_y = source["width"], source["height"]
        energy = max(source["power"])
        light.energy = energy
        light.color = [p / energy if energy else 0 for p in source["power"]]
        light.normalize = True
        obj = bpy.data.objects.new(light.name, light)
        scene.collection.objects.link(obj)
        obj.matrix_world = matrix(source["transform"])
    scene.world = bpy.data.worlds.new("reference-world")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (*contract["environment"], 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1
    scene.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.compute_device_type = "OPTIX"
    prefs.get_devices()
    devices = []
    for device in prefs.devices:
        device.use = device.type == "OPTIX"
        if device.use:
            devices.append(device.name)
    if not devices:
        raise RuntimeError("参考渲染没有可用 OptiX 设备，禁止回退 CPU")
    scene.cycles.device = "GPU"
    scene.cycles.use_denoising = False
    scene.cycles.use_adaptive_sampling = False
    for name in ("samples", "seed", "max_bounces", "diffuse_bounces", "glossy_bounces", "transmission_bounces", "transparent_max_bounces"):
        setattr(scene.cycles, name, contract["render"][name])
    scene.render.resolution_x, scene.render.resolution_y = width, height
    scene.render.resolution_percentage = 100
    scene.render.pixel_aspect_x = scene.render.pixel_aspect_y = 1
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    return {"checks": checks, "device": devices, "render": contract["render"], "camera": contract["camera"],
            "geometry": "shared-runtime-triangles-after-independent-base-vertex-and-material-check"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--content-root", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--export-only", action="store_true")
    parser.add_argument("--materials-only", action="store_true", help="仅刷新材质节点记录，不渲染或改写参考图与结构检查")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    args.output.mkdir(parents=True, exist_ok=True)
    meshes, report = imported_reference(args)
    if args.materials_only:
        print("DFV_REFERENCE: MATERIALS_EXPORTED (render NOT_RUN)")
        return
    contract = json.loads(args.scene.read_text(encoding="utf-8"))
    if contract["schema"] != "dfv-material-reference-1":
        raise RuntimeError("未知场景契约版本")
    check = construct_scene(contract, meshes)
    check["scene_sha256"] = hashlib.sha256(args.scene.read_bytes()).hexdigest()
    check["input_sha256"] = hashlib.sha256(args.file.read_bytes()).hexdigest()
    check["status"] = "STRUCTURE_PASS"
    write(args.output / "reference-check.json", check)
    if args.export_only:
        print("DFV_REFERENCE: STRUCTURE_PASS (render NOT_RUN)")
        return
    scene = bpy.context.scene
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"
    scene.render.filepath = str((args.output / "reference.exr").resolve())
    bpy.ops.render.render(write_still=True)
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_depth = "8"
    bpy.data.images["Render Result"].save_render(str((args.output / "reference.png").resolve()), scene=scene)
    check["render_status"] = "COMPLETE"
    write(args.output / "reference-check.json", check)
    print("DFV_REFERENCE: COMPLETE")


if __name__ == "__main__":
    main()
