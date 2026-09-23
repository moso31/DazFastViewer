"""生成带左右中指末节和躯干的多选回归夹具；副屏测试无需外部 DAZ 资产。"""
import json
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)


def write(name, value):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


corners = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
           (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
faces = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4],
         [3, 7, 6, 2], [0, 4, 7, 3], [1, 2, 6, 5]]
parts = [("figure", (0, 0, 0), 40), ("lMid3", (-100, 100, 0), 10),
         ("rMid3", (100, 100, 0), 10)]
vertices = [[center[k] + size * corner[k] for k in range(3)]
            for _, center, size in parts for corner in corners]
write("data/JointSelection/figure.dsf", {
    "node_library": [{"id": name, "type": "figure" if i == 0 else "bone",
                      **({"parent": "#figure"} if i else {})}
                     for i, (name, _, _) in enumerate(parts)],
    "geometry_library": [{"id": "geometry", "type": "polygon_mesh",
                          "vertices": {"count": 24, "values": vertices},
                          "polylist": {"count": 18, "values": [[i, 0] + [i * 8 + v for v in face]
                                                              for i in range(3) for face in faces]},
                          "polygon_groups": {"count": 3, "values": [p[0] for p in parts]},
                          "polygon_material_groups": {"count": 1, "values": ["Skin"]},
                          "default_uv_set": "#uv"}],
    "uv_set_library": [{"id": "uv", "vertex_count": 24,
                        "uvs": {"count": 24, "values": [[i % 2, (i // 2) % 2] for i in range(24)]}}],
    "modifier_library": [{"id": "skin", "skin": {"node": "#figure", "geometry": "#geometry",
                          "vertex_count": 24, "joints": [
                              {"node": "#" + name, "node_weights": {"count": 8, "values": [[i * 8 + v, 1] for v in range(8)]}}
                              for i, (name, _, _) in enumerate(parts)]}}],
})
write("scene.duf", {"scene": {
    "nodes": [{"id": f"person{i}", "url": "/data/JointSelection/figure.dsf#figure", "label": f"Figure{i}",
               "translation": [{"id": "x", "current_value": i * 400}],
               "geometries": [{"id": f"shape{i}", "url": "/data/JointSelection/figure.dsf#geometry"}]} for i in range(2)],
    "materials": [{"id": f"mat{i}", "geometry": f"#shape{i}", "groups": ["Skin"],
                   "diffuse": {"channel": {"value": [0.7, 0.3, 0.15] if i == 0 else [0.1, 0.5, 0.8]}}} for i in range(2)],
}})
write("project.json", {"version": 1, "content_roots": [root.as_posix()]})
print(root / "scene.duf")
