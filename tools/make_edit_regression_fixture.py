"""生成多选、ERC 缩放、细分增量更新的独立副屏回归场景。"""
import json
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)


def write(name, value):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


vertices = [[x * 5000, y * 5000, z * 5000] for x, y, z in
            [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
             (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]]
faces = [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4],
         [3, 7, 6, 2], [0, 4, 7, 3], [1, 2, 6, 5]]
write("data/Edit/figure.dsf", {
    "node_library": [{"id": "figure", "type": "figure"}],
    "geometry_library": [{"id": "geometry", "type": "subdivision_surface",
                          "current_subdivision_level": 1,
                          "vertices": {"count": 8, "values": vertices},
                          "polylist": {"count": 6, "values": [[0, 0] + face for face in faces]},
                          "polygon_material_groups": {"count": 1, "values": ["Skin"]},
                          "default_uv_set": "#uv"}],
    "uv_set_library": [{"id": "uv", "vertex_count": 8,
                        "uvs": {"count": 8, "values": [[(i % 2), (i // 2) % 2] for i in range(8)]}}],
    "modifier_library": [{"id": "skin", "skin": {"node": "#figure", "geometry": "#geometry", "vertex_count": 8,
                          "joints": [{"node": "#figure", "node_weights": {"count": 8, "values": [[i, 1] for i in range(8)]}}]}}],
})
write("data/Edit/Morphs/ScaleControl.dsf", {"modifier_library": [{
    "id": "ScaleControl", "parent": "/data/Edit/figure.dsf#geometry",
    "channel": {"type": "float", "label": "ScaleControl", "value": 1, "min": 0, "max": 1},
    "formulas": [{"output": "figure:#figure?scale/general", "operations": [
        {"op": "push", "url": "ScaleControl:#ScaleControl?value"}, {"op": "push", "val": -0.01}, {"op": "mult"}]}],
}]})
write("data/Edit/Morphs/A.dsf", {"modifier_library": [{
    "id": "A", "parent": "/data/Edit/figure.dsf#geometry",
    "channel": {"type": "float", "label": "A", "value": 0, "min": 0, "max": 1},
    "morph": {"vertex_count": 8, "deltas": {"count": 1, "values": [[6, 1000, 0, 0]]}},
}]})
write("scene.duf", {"scene": {
    "nodes": [{"id": f"person{i}", "url": "/data/Edit/figure.dsf#figure", "label": f"Cube{i}",
               "general_scale": {"id": "general_scale", "current_value": 0.02},
               "translation": [{"id": "x", "current_value": i * 250}],
               "geometries": [{"id": f"shape{i}", "url": "/data/Edit/figure.dsf#geometry"}]} for i in range(2)],
    "materials": [{"id": f"mat{i}", "geometry": f"#shape{i}", "groups": ["Skin"],
                   "diffuse": {"channel": {"value": [0.7, 0.3, 0.15] if i == 0 else [0.1, 0.5, 0.8]}}} for i in range(2)],
}})
write("project.json", {"version": 1, "content_roots": [root.as_posix()]})
print(root / "scene.duf")
