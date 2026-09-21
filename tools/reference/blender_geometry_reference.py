"""独立后台 Blender / Diffeomorphic DBZ 参考；不读入或替换 Runtime 顶点。"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import addon_utils
import bpy
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--content-root", action="append", required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    module = addon_utils.enable("bl_ext.user_default.import_daz", default_set=True, persistent=False)
    if module is None:
        raise RuntimeError("DAZ Importer 不可用")
    for key, value in {"contentDirs": args.content_root, "cloudDirs": [], "scale": 0.01,
                       "zup": True, "unflipped": False, "silentMode": True,
                       "rememberLastFolder": False, "useHighDef": False}.items():
        module.set_global_setting(key, value)
    result = bpy.ops.daz.import_daz_manually(filepath=str(args.file), directory=str(args.file.parent),
                                            files=[{"name": args.file.name}], fitMeshes="DBZFILE",
                                            materialMethod="EXTENDED_PRINCIPLED")
    if "FINISHED" not in result:
        raise RuntimeError(str(result))
    bpy.context.view_layer.update()
    output = {"blender": bpy.app.version_string, "importer": module.__file__, "fit_meshes": "DBZFILE",
              "input": str(args.file), "input_sha256": hashlib.sha256(args.file.read_bytes()).hexdigest(),
              "geometry_policy": "仅导出插件导入结果，未读取 Runtime 输出；保留基础拓扑，禁用细分",
              "objects": []}
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        for modifier in obj.modifiers:
            if modifier.type == "SUBSURF":
                modifier.show_viewport = False
        if len(obj.data.vertices) > 250000:
            continue
        positions = np.empty((len(obj.data.vertices), 3), dtype=np.float32)
        obj.data.vertices.foreach_get("co", positions.ravel())
        matrix = np.asarray(obj.matrix_world)
        world = positions @ matrix[:3, :3].T + matrix[:3, 3]
        output["objects"].append({"name": obj.name, "parent": obj.parent.name if obj.parent else None,
                                  "vertices": len(positions), "world": world.tolist()})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, ensure_ascii=False), encoding="utf-8")
    print("独立 DBZ 参考导出完成", flush=True)


if __name__ == "__main__":
    main()
